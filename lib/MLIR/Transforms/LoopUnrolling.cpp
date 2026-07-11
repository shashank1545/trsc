#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/Dialect/SCF/Utils/Utils.h"
#include "mlir/Pass/Pass.h"
#include "trsc/MLIR/TrscPasses.h"
#include <memory>

namespace mlir {
namespace trscd {
#define GEN_PASS_DEF_TRSCLOOPUNROLL
#include "TrscPasses.h.inc"
} // namespace trscd
} // namespace mlir

using namespace mlir;

namespace {

struct TrscLoopUnrollPass
    : public trscd::impl::TrscLoopUnrollBase<TrscLoopUnrollPass> {
  using Base::Base;

  void runOnOperation() override {
    if (unrollFactor <= 1)
      return;

    // Collect first: unrolling invalidates the walk.
    SmallVector<scf::ForOp> loops;
    getOperation().walk([&](scf::ForOp forOp) {
      // GEMM lowering already unrolled/tiled its loops for the target.
      if (trscd::isGemmGenerated(forOp))
        return;
      if (isInnermost(forOp))
        loops.push_back(forOp);
    });

    // Best effort: loops the utility cannot unroll are left untouched.
    for (scf::ForOp forOp : loops)
      (void)loopUnrollByFactor(forOp, unrollFactor);
  }

private:
  static bool isInnermost(scf::ForOp forOp) {
    bool innermost = true;
    forOp.getBody()->walk([&](scf::ForOp) {
      innermost = false;
      return WalkResult::interrupt();
    });
    return innermost;
  }
};

} // namespace

std::unique_ptr<mlir::Pass> mlir::trscd::createTrscLoopUnroll() {
  return std::make_unique<TrscLoopUnrollPass>();
}
