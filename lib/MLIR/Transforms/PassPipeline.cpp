#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Pass/PassManager.h"
#include "mlir/Transforms/Passes.h"
#include "trsc/MLIR/TrscPasses.h"

namespace mlir {
  namespace trscd {

    void buildCleanupPipeline(mlir::OpPassManager &pm) {
      pm.addPass(mlir::createCanonicalizerPass());
      pm.addPass(mlir::createCSEPass());
      pm.addPass(mlir::createMem2Reg());
    }

    void buildLoopOptPipeline(mlir::OpPassManager &pm) {
      pm.addNestedPass<mlir::func::FuncOp>(createTrscLICM());
    }

    void buildLoweringPipeline(mlir::OpPassManager &pm) {

    }
  } // namespace trscd
} // namespace mlir
