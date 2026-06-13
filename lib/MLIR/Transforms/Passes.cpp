#include "trsc/MLIR/TrscPasses.h"

using namespace trsc;
using namespace mlir;

#define GEN_PASS_REGISTRATION
#include "TrscPasses.h.inc"

void trsc::registerTrscPasses() {
  registerPassesPasses();
};

