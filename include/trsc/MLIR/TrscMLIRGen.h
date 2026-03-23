#ifndef TRSC_MLIR_TRSCMLIRGEN_H
#define TRSC_MLIR_TRSCMLIRGEN_H

#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/BuiltinTypeInterfaces.h"
#include "mlir/IR/MLIRContext.h"
#include "mlir/IR/OwningOpRef.h"

#include "mlir/IR/Types.h"
#include "mlir/IR/Value.h"
#include "mlir/Support/LLVM.h"
#include "trsc/AST/AST.h"
#include "trsc/AST/ASTContext.h"
#include "trsc/AST/ExprVisitor.h"
#include "trsc/AST/QualType.h"
#include "trsc/Sema/SymbolTable.h"
#include <vector>

namespace mlir {
  class MLIRContext;
  class ModuleOp;
}

namespace trsc {

  class Program;
  class MLIRGen : public ExprVisitor<MLIRGen, mlir::Value>{
    public:
      MLIRGen(mlir::MLIRContext &MLIRCtx, trsc::ASTContext &ASTCtx, 
          trsc::SymbolTable &ST);

      mlir::OwningOpRef<mlir::ModuleOp> genModule(trsc::Program &Prog);

      void genParams(const std::vector<FuncDecl::Param>& Params);
      void genFuncDecl(FuncDecl *Node);
      void genBlockStmt(BlockStmt *Stmt);
      void genLetStmt(LetStmt *Node);
      void genIfStmt(IfStmt *Node);
      void genWhileStmt(WhileStmt *Node);
      void genForStmt(ForStmt *Node);
      void genAssignment(BinExpr *Node);
      void genExprStmt(ExprStmt *Node);
      void genReturnStmt(ReturnStmt *Node);
      void genProgram(Program *Node);
      void genStmt(Stmt *Node);

      mlir::Value visitFunCall(FunCall *Node);
      mlir::Value visitIntExpr(IntExpr *Node);
      mlir::Value visitFloatExpr(FloatExpr *Node);
      mlir::Value visitVarExpr(VarExpr *Node);
      mlir::Value visitRefrExpr(RefrExpr *Node);
      mlir::Value visitBoolExpr(BoolExpr *Node);
      mlir::Value visitBinExpr(BinExpr *Node);

    private:
      mlir::MLIRContext &MLIRCtx;
      trsc::ASTContext &ASTCtx;
      trsc::SymbolTable &ST;
      
      mlir::OpBuilder Builder;
      mlir::ModuleOp Module;
      mlir::Block *CurrentEntryBlock = nullptr;

      mlir::Type ToMLIRType(QualType T);
      mlir::MemRefType ToMemRefType(QualType T);
      llvm::APFloat ToAPFloat(double D);
      mlir::Value getLValueMemRef(Expr *E);
      bool isLValue(Expr *E);

  };

} // namespace trsc

#endif // TRSC_MLIR_TRSCMLIRGEN_H
