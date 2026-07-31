#ifndef TRSC_SEMA_TYPECHECKER_H
#define TRSC_SEMA_TYPECHECKER_H

#include "trsc/AST/ASTVisitor.h"

namespace trsc {
class SemanticAnalyzer;
class DiagnosticsEngine;
class SymbolTable;

class TypeChecker : public ASTVisitor<TypeChecker> {
public:
  using ASTVisitor<TypeChecker>::visit;
  // Reads the Symbol * NameResolver cached on each VarExpr, so it needs no
  // SymbolTable of its own.
  TypeChecker(DiagnosticsEngine &Diags, ASTContext &Ctx);

  void visitIntExpr(IntExpr *Node);
  void visitFloatExpr(FloatExpr *Node);
  void visitVarExpr(VarExpr *Node);
  void visitBoolExpr(BoolExpr *Node);
  void visitASExpr(ASExpr *Node);
  void visitArrayExpr(ArrayExpr *Node);
  void visitArrayAccessExpr(ArrayAccessExpr *Node);
  void visitRefrExpr(RefrExpr *Node);
  void visitBinExpr(BinExpr *Node);
  void visitLetStmt(LetStmt *Node);
  void visitIfStmt(IfStmt *Node);
  void visitWhileStmt(WhileStmt *Node);
  void visitForStmt(ForStmt *Node);
  void visitExprStmt(ExprStmt *Node);
  void visitReturnStmt(ReturnStmt *Node);
  void visitFuncDecl(FuncDecl *Node);
  void visitFunCall(FunCall *Node);

  QualType resolveType(Type *Node);

private:
  DiagnosticsEngine &Diags;
  ASTContext &Ctx;
  QualType CurrentFunctionReturnType;
  QualType ExpectedType;
};

} // namespace trsc

#endif // TRSC_SEMA_TYPECHECKER_H
