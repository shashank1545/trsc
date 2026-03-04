#ifndef TRSC_MLIR_TRSCMLIRGEN_H
#define TRSC_MLIR_TRSCMLIRGEN_H

#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/MLIRContext.h"
#include "mlir/IR/OwningOpRef.h"

#include "trsc/AST/AST.h"
#include "trsc/AST/ASTContext.h"
#include "trsc/Sema/SymbolTable.h"

using namespace mlir;

namespace trsc {

  class Program;
  class MLIRGen {
    public:
      MLIRGen(mlir::MLIRContext &MLIRCtx, trsc::ASTContext &ASTCtx, 
          trsc::SymbolTable &ST);

      mlir::OwningOpRef<mlir::ModuleOp> generate(trsc::Program &Prog);

    private:
      mlir::MLIRContext &MLIRCtx;
      trsc::ASTContext &ASTCtx;
      trsc::SymbolTable &ST;
  };

} // namespace trsc

#endif // TRSC_MLIR_TRSCMLIRGEN_H
