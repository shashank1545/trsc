#include "mlir/Pass/Pass.h"
#include "mlir/IR/BuiltinOps.h"
#include "llvm/ADT/StringRef.h"
#include "trsc/MLIR/TrscDialect.h"
#include "trsc/MLIR/TrscOps.h"
#include "trsc/MLIR/TrscPasses.h"

using namespace trsc;
using namespace mlir;

struct LowerTrscToStandardPass
    : public PassWrapper<LowerTrscToStandardPass, OperationPass<ModuleOp>> {
  
  StringRef getArgument() const final {
    return "lower-trsc-to-standard";
  }
  StringRef getDescription() const final {
    return "Lower the TRSC dialect to standard dialects.";
  }

  void runOnOperation() override {
    // The actual lowering logic will go here.
    // For now, this pass does nothing.
  }
};

