#ifndef TRSC_MLIR_TRSCMLIRGEN_H
#define TRSC_MLIR_TRSCMLIRGEN_H

#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/MLIRContext.h"
#include "mlir/IR/OwningOpRef.h"

#include "mlir/IR/Types.h"
#include "trsc/AST/AST.h"
#include "trsc/AST/ASTContext.h"
#include "trsc/AST/ASTVisitor.h"
#include "trsc/AST/QualType.h"
#include "trsc/Sema/SymbolTable.h"

namespace mlir {
  class MLIRContext;
  class ModuleOp;
}

namespace trsc {

  class Program;
  class MLIRGen : public ASTVisitor<MLIRGen>{
    public:
      MLIRGen(mlir::MLIRContext &MLIRCtx, trsc::ASTContext &ASTCtx, 
          trsc::SymbolTable &ST);

      mlir::OwningOpRef<mlir::ModuleOp> generate(trsc::Program &Prog);

      void visitProgram(Program *Node);
      void visitFuncDecl(FuncDecl *Node);
      void visitBlockStmt(BlockStmt *Stmt);

    private:
      mlir::MLIRContext &MLIRCtx;
      trsc::ASTContext &ASTCtx;
      trsc::SymbolTable &ST;
      
      mlir::OpBuilder Builder;
      mlir::ModuleOp Module;

      mlir::Type convertType(QualType T);

  };

} // namespace trsc

#endif // TRSC_MLIR_TRSCMLIRGEN_H
