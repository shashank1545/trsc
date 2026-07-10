#include "mlir/Pass/PassManager.h"
#include "trsc/MLIR/MatMulOpts/MatMulOptPasses.h"

#include "mlir/Dialect/Func/IR/FuncOps.h"

namespace mlir {
namespace trscd {

void buildMatMulOptPipeline(mlir::PassManager &pm, int optLevel,
                            AutoTuneConfig tuneConfig) {
  // Phase 1: Recognize matmul patterns from both entry points
  pm.nest<func::FuncOp>().addPass(createMatMulRecognitionPass());

  // Preserve producer-consumer locality before lowering GEMM to loops/kernels.
  pm.nest<func::FuncOp>().addPass(createGemmEpilogueFusionPass());

  // Phase 2: Compute parameters if optimizing
  if (optLevel >= 9) {
    pm.nest<func::FuncOp>().addPass(createAutoTuningPass(tuneConfig));
  }

  // Phase 3: Single lowering pass that emits standard MLIR based on level
  pm.nest<func::FuncOp>().addPass(createLowerTrscdMatMulPass(optLevel));
}

} // namespace trscd
} // namespace mlir
