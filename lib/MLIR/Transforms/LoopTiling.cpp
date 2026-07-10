#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/Dialect/SCF/Utils/Utils.h"
#include "mlir/Pass/Pass.h"
#include "trsc/MLIR/TrscPasses.h"
#include <memory>

namespace mlir {
  namespace trscd {
#define GEN_PASS_DEF_TRSCLOOPTILE
#include "TrscPasses.h.inc"
  } // namespace trscd
} // namespace mlir

using namespace mlir;

namespace {

struct TrscLoopTilePass
    : public trscd::impl::TrscLoopTileBase<TrscLoopTilePass> {
  using Base::Base;

  void runOnOperation() override {
    if (tileSize <= 1)
      return;

    // Collect outermost roots of bands first: tiling invalidates the walk.
    SmallVector<scf::ForOp> roots;
    getOperation().walk([&](scf::ForOp forOp) {
      // GEMM lowering already tiled its loops for the target.
      if (trscd::isGemmGenerated(forOp))
        return;
      // Only the outermost loop of a nest can start a band.
      if (forOp->getParentOfType<scf::ForOp>())
        return;
      roots.push_back(forOp);
    });

    for (scf::ForOp root : roots) {
      SmallVector<scf::ForOp> band;
      getPerfectlyNestedLoops(band, root);
      // Tiling a single loop is just strip-mining; only tile real nests
      // where the interchange improves locality.
      if (band.size() < 2)
        continue;

      OpBuilder builder(root);
      Value size = arith::ConstantIndexOp::create(builder, root.getLoc(),
                                                  tileSize);
      SmallVector<Value> sizes(band.size(), size);
      tilePerfectlyNested(root, sizes);
    }
  }
};

} // namespace

std::unique_ptr<mlir::Pass> mlir::trscd::createTrscLoopTile() {
  return std::make_unique<TrscLoopTilePass>();
}
