#ifndef TRSC_MLIR_TRSCPASSES_H 
#define TRSC_MLIR_TRSCPASSES_H 

#include "mlir/Pass/Pass.h"
#include <memory>

namespace mlir {
  namespace trscd {

    std::unique_ptr<mlir::Pass> createTrscLICM();
    std::unique_ptr<mlir::Pass> createTrscLoopFusion();
    std::unique_ptr<mlir::Pass> optimizeStandard();

    void registerTrscPasses();
    void registerOptStdPasses();

    #define GEN_PASS_DECL
    #include "TrscPasses.h.inc"

  } // namespace trscd
} // namespace mlir
#endif // TRSC_MLIR_TRSCPASSES_H 
