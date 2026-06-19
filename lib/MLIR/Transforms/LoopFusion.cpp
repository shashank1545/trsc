#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "trsc/MLIR/TrscPasses.h"
#include "trsc/MLIR/Utils.h"

namespace mlir {
  namespace trscd {
#define GEN_PASS_DEF_TRSCLOOPFUSION
#include "TrscPasses.h.inc"
  } // namespace trscd
} // namespace mlir

using namespace mlir;

struct TrscLoopFusionPass : 
  public trscd::impl::TrscLoopFusionBase<TrscLoopFusionPass> {
  void runOnOperation() override {
    mlir::func::FuncOp funcOp = getOperation();
    bool modified = false;

    funcOp.walk([&](mlir::Block *block) {
      for (auto it = block->begin(); it != block->end(); ++it) {
        auto loop1 = mlir::dyn_cast<mlir::scf::ForOp>(&*it);    
        if(!loop1) continue;
        auto nextIt = std::next(it);
        if(nextIt == block->end()) continue;
        auto loop2 = mlir::dyn_cast<mlir::scf::ForOp>(&*nextIt);
        if(!loop2) continue;

        if(loop1.getLowerBound() != loop2.getLowerBound() || 
            loop1.getUpperBound() != loop2.getUpperBound() || 
            loop1.getStep() != loop2.getStep()) continue;

        if(loop1.getNumResults()>0 && loop2.getNumResults()>0) continue;

        if(trscd::hasMemoryHazards(loop1,loop2)) continue;

        loop2.getInductionVar().replaceAllUsesWith(loop1.getInductionVar());
        mlir::Block* body1 = loop1.getBody();
        mlir::Block* body2 = loop2.getBody();

        auto &ops2 = body2->getOperations(); 
        body1->getOperations().splice(
            std::prev(body1->end()), ops2, ops2.begin(), std::prev(ops2.end()));
        loop2.erase(); 
        modified = true;
      }
      if(!modified) markAllAnalysesPreserved(); 
    });
  }
};

std::unique_ptr<mlir::Pass> trscd::createTrscLoopFusion() {
  return std::make_unique<TrscLoopFusionPass>();
}

