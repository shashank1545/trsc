#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/MLIRContext.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Transforms/GreedyPatternRewriteDriver.h"
#include "trsc/MLIR/MatMulOpts/MatMulOptPasses.h"
#include "trsc/MLIR/TrscDialect.h"
#include "trsc/MLIR/TrscOps.h"

using namespace mlir;

namespace {

/// Check if an op is structural noise we can skip when looking for loop
/// nesting.
static bool isNonComputeOp(Operation &op) {
  return isa<arith::IndexCastOp, arith::ConstantOp, memref::DimOp>(&op) ||
         op.getName().getStringRef() == "ub.poison";
}

/// Find the single scf::ForOp in a block, skipping noise ops.
static scf::ForOp getSingleInnerForOp(Block *block) {
  scf::ForOp result = nullptr;
  for (Operation &op : block->without_terminator()) {
    if (auto forOp = dyn_cast<scf::ForOp>(&op)) {
      if (result)
        return nullptr; // multiple for loops
      result = forOp;
    } else if (!isNonComputeOp(op)) {
      return nullptr;
    }
  }
  return result;
}

/// Trace a value back through arith.index_cast chains to find the root.
/// This lets us compare indices that go through i32↔index round-trips.
static Value getRootIndex(Value v) {
  while (auto castOp = v.getDefiningOp<arith::IndexCastOp>())
    v = castOp.getIn();
  return v;
}

/// Match the post-cleanup matmul pattern where all compute ops are in the
/// innermost loop body (the form produced by canonicalize + CSE + mem2reg):
///
///   scf.for %i ... {              // may have iter_args (ignored)
///     scf.for %j ... {
///       scf.for %k ... {
///         %c  = memref.load C[f(i), f(j)]
///         %a  = memref.load A[f(i), f(k)]
///         %b  = memref.load B[f(k), f(j)]
///         %m  = arith.mulf %a, %b
///         %s  = arith.addf %c, %m
///         memref.store %s, C[f(i), f(j)]
///       }
///     }
///   }
///
/// where f() represents possible arith.index_cast chains.
///
struct RecognizeMatMulPattern : public OpRewritePattern<scf::ForOp> {
  using OpRewritePattern<scf::ForOp>::OpRewritePattern;

  LogicalResult matchAndRewrite(scf::ForOp loopI,
                                PatternRewriter &rewriter) const override {

    // --- Step 1: Find three nested loops i → j → k ---
    auto loopJ = getSingleInnerForOp(loopI.getBody());
    if (!loopJ)
      return failure();

    auto loopK = getSingleInnerForOp(loopJ.getBody());
    if (!loopK)
      return failure();

    // --- Step 2: Classify ops in the innermost loop body ---
    SmallVector<memref::LoadOp, 3> loads;
    memref::StoreOp storeOp;
    arith::MulFOp mulOp;
    arith::AddFOp addOp;

    for (Operation &op : loopK.getBody()->without_terminator()) {
      if (isNonComputeOp(op)) {
        continue;
      } else if (auto load = dyn_cast<memref::LoadOp>(&op)) {
        loads.push_back(load);
      } else if (auto mul = dyn_cast<arith::MulFOp>(&op)) {
        if (mulOp)
          return failure(); // multiple muls
        mulOp = mul;
      } else if (auto add = dyn_cast<arith::AddFOp>(&op)) {
        if (addOp)
          return failure(); // multiple adds
        addOp = add;
      } else if (auto store = dyn_cast<memref::StoreOp>(&op)) {
        if (storeOp)
          return failure(); // multiple stores
        storeOp = store;
      } else {
        return failure(); // unexpected op
      }
    }

    // Must have exactly 3 loads (C, A, B), 1 store, 1 mul, 1 add.
    if (loads.size() != 3 || !storeOp || !mulOp || !addOp)
      return failure();

    // --- Step 3: Verify dataflow: store(add(loadC, mul(loadA, loadB))) ---
    if (storeOp.getValueToStore() != addOp.getResult())
      return failure();

    Value mulRes = mulOp.getResult();
    Value loadCRes;
    if (addOp.getLhs() == mulRes)
      loadCRes = addOp.getRhs();
    else if (addOp.getRhs() == mulRes)
      loadCRes = addOp.getLhs();
    else
      return failure();

    // Identify which load is C, A, B from the dataflow.
    memref::LoadOp loadA, loadB, loadC;
    for (auto load : loads) {
      if (load.getResult() == loadCRes)
        loadC = load;
      else if (load.getResult() == mulOp.getLhs())
        loadA = load;
      else if (load.getResult() == mulOp.getRhs())
        loadB = load;
    }
    if (!loadA || !loadB || !loadC)
      return failure();

    // --- Step 4: Verify index patterns using getRootIndex ---
    Value ivI = loopI.getInductionVar();
    Value ivJ = loopJ.getInductionVar();
    Value ivK = loopK.getInductionVar();

    auto checkIdx = [](memref::LoadOp load, Value idx0, Value idx1) {
      auto indices = load.getIndices();
      return indices.size() == 2 &&
             getRootIndex(indices[0]) == getRootIndex(idx0) &&
             getRootIndex(indices[1]) == getRootIndex(idx1);
    };

    // C[i, j]
    if (!checkIdx(loadC, ivI, ivJ))
      return failure();

    // A[i, k] and B[k, j] — try both orderings
    if (!checkIdx(loadA, ivI, ivK) || !checkIdx(loadB, ivK, ivJ)) {
      std::swap(loadA, loadB);
      if (!checkIdx(loadA, ivI, ivK) || !checkIdx(loadB, ivK, ivJ))
        return failure();
    }

    // Store must write to C[i, j] on the same memref as loadC.
    auto storeIndices = storeOp.getIndices();
    if (storeIndices.size() != 2 ||
        getRootIndex(storeIndices[0]) != getRootIndex(ivI) ||
        getRootIndex(storeIndices[1]) != getRootIndex(ivJ))
      return failure();
    if (storeOp.getMemRef() != loadC.getMemRef())
      return failure();

    // --- Step 5: All checks passed — emit trscd.gemm ---
    rewriter.setInsertionPoint(loopI);
    trscd::GemmOp::create(rewriter, loopI.getLoc(), loadA.getMemRef(),
                          loadB.getMemRef(), loadC.getMemRef(),
                          rewriter.getF32FloatAttr(1.0f), // alpha
                          rewriter.getF32FloatAttr(1.0f), // beta
                          nullptr,                        // M
                          nullptr,                        // N
                          nullptr,                        // K
                          nullptr                         // tiling_params
    );

    rewriter.eraseOp(loopI);
    return success();
  }
};

struct MatMulRecognitionPass
    : public PassWrapper<MatMulRecognitionPass, OperationPass<func::FuncOp>> {
  MLIR_DEFINE_EXPLICIT_INTERNAL_INLINE_TYPE_ID(MatMulRecognitionPass)

  StringRef getArgument() const final { return "matmul-recognize"; }
  StringRef getDescription() const final {
    return "Recognize matrix multiplication loops and convert to trscd.gemm";
  }

  void runOnOperation() override {
    RewritePatternSet patterns(&getContext());
    patterns.add<RecognizeMatMulPattern>(&getContext());
    if (failed(applyPatternsGreedily(getOperation(), std::move(patterns)))) {
      signalPassFailure();
    }
  }
};
} // namespace

namespace mlir {
namespace trscd {

std::unique_ptr<mlir::Pass> createMatMulRecognitionPass() {
  return std::make_unique<MatMulRecognitionPass>();
}

} // namespace trscd
} // namespace mlir
