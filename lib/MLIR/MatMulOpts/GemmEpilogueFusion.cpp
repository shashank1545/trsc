#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/Pass/Pass.h"
#include "trsc/MLIR/MatMulOpts/MatMulOptPasses.h"
#include "trsc/MLIR/TrscOps.h"

using namespace mlir;

namespace {

static bool isStructuralNoise(Operation &op) {
  return isa<arith::ConstantOp, arith::IndexCastOp, memref::DimOp>(&op) ||
         op.getName().getStringRef() == "ub.poison";
}

static scf::ForOp getOnlyLoop(Block &block) {
  scf::ForOp result;
  for (Operation &op : block.without_terminator()) {
    if (auto loop = dyn_cast<scf::ForOp>(op)) {
      if (result)
        return {};
      result = loop;
    } else if (!isStructuralNoise(op)) {
      return {};
    }
  }
  return result;
}

static Value rootIndex(Value value) {
  while (auto cast = value.getDefiningOp<arith::IndexCastOp>())
    value = cast.getIn();
  return value;
}

static bool hasIndices(memref::LoadOp load, Value first, Value second) {
  auto indices = load.getIndices();
  return indices.size() == 2 && rootIndex(indices[0]) == rootIndex(first) &&
         rootIndex(indices[1]) == rootIndex(second);
}

static bool isZero(Value value) {
  auto constant = value.getDefiningOp<arith::ConstantOp>();
  if (!constant)
    return false;
  auto attr = dyn_cast<FloatAttr>(constant.getValue());
  return attr && attr.getValue().isZero();
}

/// Fuses the canonical post-GEMM elementwise consumer:
///
///   for i, j: C[i, j] = max(C[i, j] + bias[j], 0.0)
///
/// The fused operation carries the bias operand and the activation flag.  The
/// lowering pass then emits this work next to the GEMM accumulator store,
/// avoiding an additional full traversal of C.
struct GemmEpilogueFusionPass
    : public PassWrapper<GemmEpilogueFusionPass,
                         OperationPass<func::FuncOp>> {
  MLIR_DEFINE_EXPLICIT_INTERNAL_INLINE_TYPE_ID(GemmEpilogueFusionPass)

  StringRef getArgument() const final { return "gemm-epilogue-fusion"; }
  StringRef getDescription() const final {
    return "Fuse column bias-add and ReLU consumers into trscd.gemm";
  }

  void runOnOperation() override {
    SmallVector<trscd::GemmOp> gemms;
    getOperation().walk([&](trscd::GemmOp gemm) { gemms.push_back(gemm); });

    for (trscd::GemmOp gemm : gemms) {
      Operation *next = gemm->getNextNode();
      while (next && isStructuralNoise(*next))
        next = next->getNextNode();
      auto loopI = dyn_cast_or_null<scf::ForOp>(next);
      if (!loopI)
        continue;
      auto loopJ = getOnlyLoop(*loopI.getBody());
      if (!loopJ)
        continue;

      SmallVector<memref::LoadOp> loads;
      memref::StoreOp store;
      arith::AddFOp add;
      arith::MaximumFOp relu;
      bool unsupported = false;
      for (Operation &op : loopJ.getBody()->without_terminator()) {
        if (isStructuralNoise(op))
          continue;
        if (auto load = dyn_cast<memref::LoadOp>(op))
          loads.push_back(load);
        else if (auto candidate = dyn_cast<arith::AddFOp>(op)) {
          if (add) {
            unsupported = true;
            break;
          }
          add = candidate;
        } else if (auto candidate = dyn_cast<arith::MaximumFOp>(op)) {
          if (relu) {
            unsupported = true;
            break;
          }
          relu = candidate;
        } else if (auto candidate = dyn_cast<memref::StoreOp>(op)) {
          if (store) {
            unsupported = true;
            break;
          }
          store = candidate;
        } else {
          unsupported = true;
          break;
        }
      }

      if (unsupported || loads.size() != 2 || !store || !add || !relu ||
          store.getValueToStore() != relu.getResult() ||
          !((relu.getLhs() == add.getResult() && isZero(relu.getRhs())) ||
            (relu.getRhs() == add.getResult() && isZero(relu.getLhs()))))
        continue;

      if (store.getMemRef() != gemm.getC() ||
          store.getIndices().size() != 2 ||
          rootIndex(store.getIndices()[0]) != rootIndex(loopI.getInductionVar()) ||
          rootIndex(store.getIndices()[1]) != rootIndex(loopJ.getInductionVar()))
        continue;

      memref::LoadOp loadC, loadBias;
      for (memref::LoadOp load : loads) {
        if (hasIndices(load, loopI.getInductionVar(), loopJ.getInductionVar()))
          loadC = load;
        else if (load.getIndices().size() == 1 &&
                 rootIndex(load.getIndices()[0]) == rootIndex(loopJ.getInductionVar()))
          loadBias = load;
      }
      if (!loadC || !loadBias || loadC.getMemRef() != gemm.getC() ||
          !((add.getLhs() == loadC.getResult() &&
             add.getRhs() == loadBias.getResult()) ||
            (add.getRhs() == loadC.getResult() &&
             add.getLhs() == loadBias.getResult())))
        continue;

      OpBuilder builder(gemm);
      OperationState state(gemm.getLoc(), trscd::GemmOp::getOperationName());
      state.addOperands({gemm.getA(), gemm.getB(), gemm.getC(),
                         loadBias.getMemRef()});
      for (NamedAttribute attr : gemm->getAttrs())
        if (attr.getName().getValue() != "relu")
          state.addAttribute(attr.getName(), attr.getValue());
      state.addAttribute("relu", builder.getBoolAttr(true));
      auto fused = cast<trscd::GemmOp>(builder.create(state));
      (void)fused;
      loopI.erase();
      gemm.erase();

    }
  }
};

} // namespace

namespace mlir::trscd {

std::unique_ptr<mlir::Pass> createGemmEpilogueFusionPass() {
  return std::make_unique<GemmEpilogueFusionPass>();
}

void registerMatMulOptPasses() {
  PassRegistration<GemmEpilogueFusionPass>();
}

} // namespace mlir::trscd
