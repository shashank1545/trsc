#include "trsc/MLIR/TrscPasses.h"

namespace mlir {
  namespace trscd {
#define GEN_PASS_REGISTRATION
#include "TrscPasses.h.inc"
  }
}

void mlir::trscd::registerTrscPasses() {
  registerPassesPasses();
};

