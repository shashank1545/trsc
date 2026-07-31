#ifndef TRSC_SEMA_BORROWCHECKER_H
#define TRSC_SEMA_BORROWCHECKER_H

#include "trsc/AST/ASTVisitor.h"

namespace trsc {
class SemanticAnalyzer;
class DiagnosticsEngine;
class SymbolTable;
class ASTContext;

class BorrowChecker : public ASTVisitor<BorrowChecker> {
public:
  using ASTVisitor<BorrowChecker>::visit;
  BorrowChecker(DiagnosticsEngine &Diags, ASTContext &Ctx);

  void visitLetStmt(LetStmt *Node);
  void visitVarExpr(VarExpr *Node);
  void visitBinExpr(BinExpr *Node);
  void visitIfStmt(IfStmt *Node);
  void visitWhileStmt(WhileStmt *Node);
  void visitFuncDecl(FuncDecl *Node);
  void visitFunCall(FunCall *Node);
  void visitReturnStmt(ReturnStmt *Node);

private:
  DiagnosticsEngine &Diags;
  ASTContext &Ctx;
};

} // namespace trsc

#endif // TRSC_SEMA_BORROWCHECKER_H
