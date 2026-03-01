#ifndef TRSC_SEMA_NAMERESOLVER_H
#define TRSC_SEMA_NAMERESOLVER_H

#include "trsc/AST/AST.h"
#include "trsc/AST/ASTVisitor.h"
#include "trsc/Sema/SymbolTable.h"
#include "trsc/Basic/Diagnostics.h"

namespace trsc {

  class NameResolver : public ASTVisitor<NameResolver> {
    private:
      SymbolTable &ST;
      DiagnosticsEngine &Diags;

    public:
      NameResolver(DiagnosticsEngine &Diags, SymbolTable &ST)
        :  ST(ST), Diags(Diags) {}

      void visitProgram(Program *P);

      void visitBlockStmt(BlockStmt *S);
      void visitFuncDecl(FuncDecl *D);
      void visitLetStmt(LetStmt *S);
      void visitExprStmt(ExprStmt *S);
      void visitVarExpr(VarExpr *E);
      void visitIntExpr(IntExpr *E);
      void visitFloatExpr(FloatExpr *E);
      void visitRangeExpr(RangeExpr *E);
      void visitFunCall(FunCall *E);
      void visitReturnStmt(ReturnStmt *S);
      void visitForStmt(ForStmt *S); 
      void visitWhileStmt(WhileStmt *S);
      void visitIfStmt(IfStmt *S);

  };
} // namespace trsc

#endif // TRSC_SEMA_NAMERESOLVER_H
