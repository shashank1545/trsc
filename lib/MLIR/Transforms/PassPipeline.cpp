#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Pass/PassManager.h"
#include "mlir/Transforms/Passes.h"
#include "trsc/MLIR/TrscPasses.h"

#include "mlir/Dialect/GPU/Pipelines/Passes.h"
#include "mlir/Dialect/Linalg/Passes.h"

namespace mlir {
  namespace trscd {

    void buildCleanupPipeline(mlir::OpPassManager &pm) {
      pm.addPass(mlir::createCanonicalizerPass());
      pm.addPass(mlir::createCSEPass());
      pm.addPass(mlir::createMem2Reg());
    }

    void buildLoopOptPipeline(mlir::OpPassManager &pm) {
      pm.addNestedPass<mlir::func::FuncOp>(createTrscLICM());
      pm.addNestedPass<mlir::func::FuncOp>(createTrscLoopFusion());
    }

    void buildLoweringPipeline(mlir::OpPassManager &pm) {
      // NVVM pipeline never lowers linalg; turn it into loops first.
      pm.addPass(mlir::createConvertLinalgToLoopsPass());
      mlir::gpu::GPUToNVVMPipelineOptions options;
      options.cubinFormat = "isa";
      options.optLevel = 3;
      mlir::gpu::buildLowerToNVVMPassPipeline(pm, options);
    }
  } // namespace trscd
} // namespace mlir
