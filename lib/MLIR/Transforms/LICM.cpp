#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Interfaces/SideEffectInterfaces.h"
#include "mlir/Pass/Pass.h"
#include "trsc/MLIR/TrscPasses.h"
#include <memory>

namespace mlir {
  namespace trscd {
#define GEN_PASS_DEF_TRSCLICM
#include "TrscPasses.h.inc"
  } // namespace trscd
} // namespace mlir

using namespace mlir;

struct TrscLICMPass : public trscd::impl::TrscLICMBase<TrscLICMPass> {

  void runOnOperation( ) override{
    func::FuncOp funcOp = getOperation();

    funcOp.walk([](scf::ForOp forOp) {
      bool changed = true;
      while(changed) {
        changed = false;
        llvm::SmallVector<Operation* , 8> toHoist;

        for (auto& op: forOp.getBody()->without_terminator()) {
          if(isLoopInvariant(&op, forOp)) toHoist.push_back(&op);
        }

        for (auto *op: toHoist) {
          op->moveBefore(forOp);
          changed = true;
        }
      }
    });
  }

  private:
    static bool isLoopInvariant(Operation *op, scf::ForOp forOp) {
      if(!isPure(op))
        return false;
      
      for (Value operand : op->getOperands()) {
        if (operand == forOp.getInductionVar()) 
          return false;

        if (auto *defOp = operand.getDefiningOp()) {
          if (forOp.getBody()->findAncestorOpInBlock(*defOp))
            return false;
        }

        if (auto blockArg = dyn_cast<BlockArgument>(operand)) {
          if (blockArg.getOwner() == forOp.getBody())
            return false;
        }
      }

      return true;
    }
};

std::unique_ptr<mlir::Pass> trscd::createTrscLICM() {
  return std::make_unique<TrscLICMPass>();
}
