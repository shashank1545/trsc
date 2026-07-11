#include "trsc/MLIR/TrscDialect.h"
#include "trsc/MLIR/TrscOps.h"

#include "TrscDialect.cpp.inc"

void mlir::trscd::TrscDialect::initialize() {
  addOperations<
#define GET_OP_LIST
#include "TrscOps.cpp.inc"
      >();
}
