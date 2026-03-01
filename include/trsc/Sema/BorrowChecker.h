#ifndef TRSC_SEMA_BORROWCHECKER_H
#define TRSC_SEMA_BORROWCHECKER_H

#include "trsc/AST/ASTContext.h"
#include "trsc/AST/ASTVisitor.h"
#include "trsc/Basic/Diagnostics.h"
#include "trsc/Sema/SymbolTable.h"

namespace trsc {
class SemanticAnalyzer;

class BorrowChecker : public ASTVisitor<BorrowChecker> {
public:
  using ASTVisitor<BorrowChecker>::visit;
  BorrowChecker(DiagnosticsEngine &Diags, SymbolTable &ST, ASTContext &Ctx);

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
  SymbolTable &ST;
  ASTContext &Ctx;
};

} // namespace trsc

#endif // TRSC_SEMA_BORROWCHECKER_H
