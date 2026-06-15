#ifndef TRSC_MLIR_TRSCPASSES_H 
#define TRSC_MLIR_TRSCPASSES_H 

#include "mlir/Pass/Pass.h"
#include <memory>

namespace trsc {

  std::unique_ptr<mlir::Pass> createTrscLICM();
  std::unique_ptr<mlir::Pass> optimizeStandard();

  void registerTrscPasses();
  void registerOptStdPasses();

#define GEN_PASS_DECL
#include "TrscPasses.h.inc"
}
#endif // TRSC_MLIR_TRSCPASSES_H 
