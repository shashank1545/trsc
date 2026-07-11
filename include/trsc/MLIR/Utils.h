#ifndef TRSC_MLIR_UTILS_H
#define TRSC_MLIR_UTILS_H

#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/Operation.h"
#include "mlir/IR/ValueRange.h"

namespace mlir {
namespace trscd {

struct OpEffect {
  mlir::Operation *op;
  mlir::MemoryEffects::EffectInstance effect;
};

bool hasMemoryHazards(mlir::scf::ForOp loop1, mlir::scf::ForOp loop2);

std::optional<mlir::OperandRange> getIndicesIfMemRefOp(mlir::Operation *op);

bool areIndicesPerfectlyAligned(mlir::Operation *op1, mlir::Operation *op2,
                                mlir::scf::ForOp loop1, mlir::scf::ForOp loop2);
} // namespace trscd
} // namespace mlir

#endif // TRSC_MLIR_UTILS_H
