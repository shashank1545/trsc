#ifndef TRSC_MLIR_TRSCOPS_H
#define TRSC_MLIR_TRSCOPS_H

#include "mlir/Interfaces/InferTypeOpInterface.h"
#include "mlir/Interfaces/SideEffectInterfaces.h"
#include "mlir/Bytecode/BytecodeOpInterface.h"

#define GET_OP_CLASSES
#include "TrscOps.h.inc"

#endif // TRSC_MLIR_TRSCOPS_H
