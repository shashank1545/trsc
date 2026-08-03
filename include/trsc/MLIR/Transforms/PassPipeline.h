#ifndef TRSC_MLIR_PASSPIPELINE_H
#define TRSC_MLIR_PASSPIPELINE_H

#include "mlir/Pass/PassManager.h"
#include "llvm/ADT/StringRef.h"

namespace mlir {
class OpPassManager;
} // namespace mlir

namespace mlir {
namespace trscd {

void buildCleanupPipeline(mlir::OpPassManager &pm);
void buildMem2RegPipeline(mlir::OpPassManager &pm);
void buildLoopOptPipeline(mlir::OpPassManager &pm);
void buildLateLoopOptPipeline(mlir::OpPassManager &pm);
void buildLoweringPipeline(mlir::OpPassManager &pm, llvm::StringRef cudaArch);

} // namespace trscd

} // namespace mlir

#endif // TRSC_MLIR_PASSPIPELINE_H
