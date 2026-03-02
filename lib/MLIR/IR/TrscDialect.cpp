#include "trsc/MLIR/TrscDialect.h"
#include "trsc/MLIR/TrscOps.h"

#include "mlir/IR/DialectImplementation.h"
#include "mlir/IR/Builders.h"

using namespace trsc;
using namespace mlir;

#include "TrscDialect.cpp.inc"

void trsc::TrscDialect::initialize() {
  addOperations<
#define GET_OP_LIST
#include "TrscOps.cpp.inc"
  >();
}

mlir::Operation *TrscDialect::materializeConstant(mlir::OpBuilder &builder,
                                                   mlir::Attribute value,
                                                   mlir::Type type,
                                                   mlir::Location loc) {
  return nullptr;
}
