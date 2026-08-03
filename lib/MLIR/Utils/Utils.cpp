#include "trsc/MLIR/Utils.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Interfaces/SideEffectInterfaces.h"

bool mlir::trscd::hasMemoryHazards(mlir::scf::ForOp loop1,
                                   mlir::scf::ForOp loop2) {
  llvm::SmallVector<OpEffect, 8> effects1;
  llvm::SmallVector<OpEffect, 8> effects2;

  loop1.walk([&](mlir::MemoryEffectOpInterface op) {
    llvm::SmallVector<mlir::MemoryEffects::EffectInstance, 4> localEffects;
    op.getEffects(localEffects);
    for (auto &eff : localEffects) {
      effects1.push_back({op, eff});
    }
  });

  loop2.walk([&](mlir::MemoryEffectOpInterface op) {
    llvm::SmallVector<mlir::MemoryEffects::EffectInstance, 4> localEffects;
    op.getEffects(localEffects);
    for (auto &eff : localEffects) {
      effects2.push_back({op, eff});
    }
  });

  for (const auto &opEff1 : effects1) {
    for (const auto &opEff2 : effects2) {

      const auto &eff1 = opEff1.effect;
      const auto &eff2 = opEff2.effect;

      if (eff1.getValue() != eff2.getValue()) {
        continue;
      }

      bool isWrite1 = llvm::isa<mlir::MemoryEffects::Write>(eff1.getEffect());
      bool isRead1 = llvm::isa<mlir::MemoryEffects::Read>(eff1.getEffect());

      bool isWrite2 = llvm::isa<mlir::MemoryEffects::Write>(eff2.getEffect());
      bool isRead2 = llvm::isa<mlir::MemoryEffects::Read>(eff2.getEffect());

      if ((isWrite1 && isRead2) || (isRead1 && isWrite2) ||
          (isWrite1 && isWrite2)) {

        if (areIndicesPerfectlyAligned(opEff1.op, opEff2.op, loop1, loop2)) {
          continue;
        }
        return true;
      }
    }
  }
  return false;
}

std::optional<mlir::OperandRange>
mlir::trscd::getIndicesIfMemRefOp(mlir::Operation *op) {
  if (auto load = mlir::dyn_cast<mlir::memref::LoadOp>(op))
    return load.getIndices();
  if (auto store = mlir::dyn_cast<mlir::memref::StoreOp>(op))
    return store.getIndices();
  return std::nullopt;
}

bool mlir::trscd::areIndicesPerfectlyAligned(mlir::Operation *op1,
                                             mlir::Operation *op2,
                                             mlir::scf::ForOp loop1,
                                             mlir::scf::ForOp loop2) {
  auto indices1 = mlir::trscd::getIndicesIfMemRefOp(op1);
  auto indices2 = mlir::trscd::getIndicesIfMemRefOp(op2);
  if (!indices1 || !indices2)
    return false;
  if (indices1->size() != indices2->size())
    return false;

  mlir::Value iv1 = loop1.getInductionVar();
  mlir::Value iv2 = loop2.getInductionVar();
  for (auto [idx1, idx2] : llvm::zip(*indices1, *indices2)) {
    if (idx1 == iv1 && idx2 == iv2) {
      continue;
    }
    if (idx1 == idx2) {
      continue;
    }
    return false;
  }
  return true;
}
