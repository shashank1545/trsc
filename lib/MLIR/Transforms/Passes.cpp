#include "trsc/MLIR/TrscPasses.h"

#include "mlir/IR/BuiltinOps.h"  
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/ControlFlow/IR/ControlFlowOps.h"

using namespace trsc;
using namespace mlir;

// Generate pass definitions
#define GEN_PASS_DEF_LOWERTRSCTOSTANDARD
#include "TrscPasses.h.inc"

void registerTrscPasses() {};

