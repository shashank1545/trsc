#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Pass/PassManager.h"
#include "mlir/Transforms/Passes.h"
#include "trsc/MLIR/TrscPasses.h"

#include "mlir/Dialect/GPU/Pipelines/Passes.h"
#include "mlir/Dialect/Linalg/Passes.h"

namespace mlir {
namespace trscd {

// Generic cleaning passes, run after every major optimization stage.
void buildCleanupPipeline(mlir::OpPassManager &pm) {
  pm.addPass(mlir::createCanonicalizerPass());
  pm.addPass(mlir::createCSEPass());
}

void buildMem2RegPipeline(mlir::OpPassManager &pm) {
  pm.addPass(mlir::createMem2Reg());
}

void buildLoopOptPipeline(mlir::OpPassManager &pm) {
  pm.addNestedPass<mlir::func::FuncOp>(createTrscLICM());
  pm.addNestedPass<mlir::func::FuncOp>(createTrscLoopFusion());
}

// Generic loop optimizations that run after GEMM recognition/lowering.
// They must not see matmul loop nests (recognition needs the canonical
// form) and skip GEMM-generated code via the trscd.gemm_generated
// marker. Tiling runs first so unrolling sees the small constant-bound
// intra-tile loops.
void buildLateLoopOptPipeline(mlir::OpPassManager &pm) {
  pm.addNestedPass<mlir::func::FuncOp>(createTrscLoopTile());
  pm.addNestedPass<mlir::func::FuncOp>(createTrscLoopUnroll());
}

void buildLoweringPipeline(mlir::OpPassManager &pm, llvm::StringRef cudaArch) {
  // NVVM pipeline never lowers linalg; turn it into loops first.
  pm.addPass(mlir::createConvertLinalgToLoopsPass());
  mlir::gpu::GPUToNVVMPipelineOptions options;
  options.cubinFormat = "isa";
  options.cubinTriple = "nvptx64-nvidia-cuda";
  options.cubinChip = cudaArch.str();
  options.optLevel = 3;
  mlir::gpu::buildLowerToNVVMPassPipeline(pm, options);
}
} // namespace trscd
} // namespace mlir
