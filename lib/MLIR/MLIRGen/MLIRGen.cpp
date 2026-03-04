
#include "trsc/MLIR/TrscMLIRGen.h"

#include "mlir/IR/Builders.h"
#include "mlir/IR/MLIRContext.h"
#include "mlir/IR/OwningOpRef.h"
#include "mlir/IR/Verifier.h"
#include "trsc/Sema/SymbolTable.h"

using namespace trsc; 
using namespace mlir;

MLIRGen::MLIRGen(mlir::MLIRContext &MLIRCtx, trsc::ASTContext &ASTCtx, 
    trsc::SymbolTable &ST) : MLIRCtx(MLIRCtx), ASTCtx(ASTCtx), ST(ST) {}

mlir::OwningOpRef<mlir::ModuleOp> MLIRGen::generate(trsc::Program &Prog) {
  auto module = mlir::OwningOpRef<::mlir::ModuleOp>(
      mlir::ModuleOp::create(mlir::UnknownLoc::get(&MLIRCtx)));


  if (failed(mlir::verify(module.get()))) {
    module.get().emitError("Module verification failed.");
    return nullptr;
  }

  return module;
}
