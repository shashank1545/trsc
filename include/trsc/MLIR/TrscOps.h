#ifndef TRSC_MLIR_TRSCOPS_H
#define TRSC_MLIR_TRSCOPS_H

#include "mlir/IR/Builders.h"
#include "mlir/IR/OpDefinition.h"
#include "mlir/IR/OpImplementation.h"
#include "mlir/IR/Operation.h"

#include "trsc/MLIR/TrscDialect.h"

#define GET_OP_CLASSES
#include "TrscOps.h.inc"

#endif // TRSC_MLIR_TRSCOPS_H
