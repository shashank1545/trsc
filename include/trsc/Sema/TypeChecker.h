#ifndef TRSC_SEMA_TYPECHECKER_H
#define TRSC_SEMA_TYPECHECKER_H

#include "trsc/AST/AST.h"
#include "trsc/AST/ASTContext.h"
#include "trsc/AST/ASTVisitor.h"
#include "trsc/AST/QualType.h"
#include "trsc/Basic/Diagnostics.h"
#include "trsc/Sema/SymbolTable.h"

namespace trsc {
class SemanticAnalyzer;

class TypeChecker : public ASTVisitor<TypeChecker> {
public:
  using ASTVisitor<TypeChecker>::visit;
  TypeChecker(DiagnosticsEngine &Diags, SymbolTable &ST, ASTContext &Ctx);

  void visitIntExpr(IntExpr *Node);
  void visitFloatExpr(FloatExpr *Node);
  void visitVarExpr(VarExpr *Node);
  void visitBoolExpr(BoolExpr *Node);
  void visitASExpr(ASExpr *Node);
  void visitRefrExpr(RefrExpr *Node);
  void visitBinExpr(BinExpr *Node);
  void visitLetStmt(LetStmt *Node);
  void visitIfStmt(IfStmt *Node);
  void visitWhileStmt(WhileStmt *Node);
  void visitForStmt(ForStmt *Node);
  void visitReturnStmt(ReturnStmt *Node);
  void visitFuncDecl(FuncDecl *Node);
  void visitFunCall(FunCall *Node);

  QualType resolveType(Type *Node);

private:
  DiagnosticsEngine &Diags;
  SymbolTable &ST;
  ASTContext &Ctx;
  QualType CurrentFunctionReturnType;
  QualType ExpectedType;
};

} // namespace trsc

#endif // TRSC_SEMA_TYPECHECKER_H
