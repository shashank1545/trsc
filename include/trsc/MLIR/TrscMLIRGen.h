#ifndef TRSC_MLIR_TRSCMLIRGEN_H
#define TRSC_MLIR_TRSCMLIRGEN_H

#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/OwningOpRef.h"
#include "trsc/AST/ExprVisitor.h"

namespace mlir {
  class MLIRContext;
  class Type;
  class ModuleOp;
  class MemRefType;
  class Block;
  class Operation;
}

namespace llvm {
  class APFloat;
  template <typename T> class SmallVectorImpl;
}

namespace trsc {

  class Program;
  class ASTContext;
  class SymbolTable;
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
      void genArrayInit(ArrayExpr *Node, mlir::Value DestMemRef, 
          QualType ArrayTy);
      void genArrayInitImpl(ArrayExpr *Node, mlir::Value DestMemRef,
          llvm::SmallVectorImpl<mlir::Value> &Indices);

      mlir::Value visitFunCall(FunCall *Node);
      mlir::Value visitIntExpr(IntExpr *Node);
      mlir::Value visitFloatExpr(FloatExpr *Node);
      mlir::Value visitVarExpr(VarExpr *Node);
      mlir::Value visitASExpr(ASExpr *Node);
      mlir::Value visitArrayAccessExpr(ArrayAccessExpr *Node);
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

      mlir::Type toMLIRType(QualType T);
      mlir::MemRefType toMemRefType(QualType T);
      const llvm::APFloat toAPFloat(double D, QualType &Type);
      mlir::Value getLValueMemRef(Expr *E);
      mlir::Value convertValueToType(mlir::Value V, mlir::Type T);
      mlir::Value getOpMemRef(mlir::Operation* E);
      bool isLValue(Expr *E);

  };

} // namespace trsc

#endif // TRSC_MLIR_TRSCMLIRGEN_H
