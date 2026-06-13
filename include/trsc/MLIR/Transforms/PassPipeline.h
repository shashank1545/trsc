#ifndef TRSC_MLIR_PASSPIPELINE_H 
#define TRSC_MLIR_PASSPIPELINE_H 

#include "mlir/Pass/PassManager.h"

namespace mlir {
  class OpPassManager; 
} // namespace mlir

namespace trsc {

  void buildCleanupPipeline(mlir::OpPassManager &pm);
  void buildLoopOptPipeline(mlir::OpPassManager &pm);
  void buildLoweringPipeline(mlir::OpPassManager &pm);

} // namespace trsc


#endif // TRSC_MLIR_PASSPIPELINE_H

