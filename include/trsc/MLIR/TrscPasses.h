#ifndef TRSC_MLIR_TRSCPASSES_H 
#define TRSC_MLIR_TRSCPASSES_H 

#include "mlir/Pass/Pass.h"
#include "mlir/IR/BuiltinOps.h"
#include <memory>

namespace trsc {

  std::unique_ptr<mlir::Pass> createLowerTrscToStandard();

  void registerTrscPasses();

#define GEN_PASS_DECL
#include "TrscPasses.h.inc"
}
#endif // TRSC_MLIR_TRSCPASSES_H 
