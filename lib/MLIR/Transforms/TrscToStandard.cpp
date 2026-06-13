#include "mlir/Pass/Pass.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "trsc/MLIR/TrscPasses.h"

namespace trsc {
#define GEN_PASS_DEF_LOWERTRSCTOSTANDARD
#include "TrscPasses.h.inc"
}

using namespace trsc;
using namespace mlir;

struct TrscToStandardPass
    : public trsc::impl::LowerTrscToStandardBase<TrscToStandardPass> {

  void runOnOperation() override {
  }
};

std::unique_ptr<mlir::Pass> trsc::createTrscToStandard() {
  return std::make_unique<TrscToStandardPass>();
}

