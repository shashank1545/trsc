#ifndef TRSC_MLIR_TRSCOPS_H
#define TRSC_MLIR_TRSCOPS_H

#include "mlir/Bytecode/BytecodeOpInterface.h"
#include "mlir/Interfaces/InferTypeOpInterface.h"
#include "mlir/Interfaces/SideEffectInterfaces.h"

#define GET_OP_CLASSES
#include "TrscOps.h.inc"

#endif // TRSC_MLIR_TRSCOPS_H
