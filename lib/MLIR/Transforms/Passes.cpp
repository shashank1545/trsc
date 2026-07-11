#include "trsc/MLIR/TrscPasses.h"

namespace mlir {
namespace trscd {
#define GEN_PASS_REGISTRATION
#include "TrscPasses.h.inc"
} // namespace trscd
} // namespace mlir

void mlir::trscd::registerTrscPasses() { registerPassesPasses(); };
