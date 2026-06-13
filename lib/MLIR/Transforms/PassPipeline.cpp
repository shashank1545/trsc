#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Pass/PassManager.h"
#include "mlir/Transforms/Passes.h"
#include "trsc/MLIR/TrscPasses.h"

namespace trsc {

  void buildCleanupPipeline(mlir::OpPassManager &pm) {
    pm.addPass(mlir::createCanonicalizerPass());
    pm.addPass(mlir::createCSEPass());
    pm.addPass(mlir::createMem2Reg());
  }

  void buildLoopOptPipeline(mlir::OpPassManager &pm) {
    pm.addNestedPass<mlir::func::FuncOp>(trsc::createTrscLICM());
  }

  void buildLoweringPipeline(mlir::OpPassManager &pm) {

  }
}
