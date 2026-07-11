#include "trsc/Sema/NameResolver.h"
#include "trsc/Basic/Diagnostics.h"
#include "trsc/Sema/SymbolTable.h"

using namespace trsc;

void NameResolver::visitProgram(Program *P) {
  ASTVisitor<NameResolver>::visitProgram(P);
}

void NameResolver::visitLetStmt(LetStmt *S) {
  S->setScope(ST.getCurrentScope());
  Symbol Sym;
  Sym.setScope(ST.getCurrentScope());
  if (!ST.addSymbol(S->getDeclaredVar()->getName(), Sym)) {
    Diags.Report(DiagKind::Error, "Redefinition of Variable",
                 S->getSourceRange().getStart());
  }
  if (S->getInitializer())
    visit(S->getInitializer());
}

void NameResolver::visitForStmt(ForStmt *S) {
  ScopedRAII Scoped(ST, ScopeKind::SCOPE_FORSTMT);
  S->setScope(ST.getCurrentScope());
  Symbol Sym;
  Sym.setScope(ST.getCurrentScope());
  if (!ST.addSymbol(S->getInit()->getName(), Sym)) {
    Diags.Report(DiagKind::Error, "Variable already defined",
                 S->getSourceRange().getStart());
  }
  S->getInit()->setScope(ST.getCurrentScope());
  if (S->getRange())
    visit(S->getRange());

  if (Stmt *Body = S->getBody()) {
    if (Body->getASTNodeKind() == ASTNodeKind::ASTK_BLOCKSTMT) {
      ASTVisitor<NameResolver>::visitBlockStmt(static_cast<BlockStmt *>(Body));
    } else {
      visit(Body);
    }
  }
}

void NameResolver::visitWhileStmt(WhileStmt *S) {
  ScopedRAII Scoped(ST, ScopeKind::SCOPE_WHILESTMT);
  S->setScope(ST.getCurrentScope());
  if (S->getCondition())
    visit(S->getCondition());

  if (Stmt *Body = S->getBody()) {
    if (Body->getASTNodeKind() == ASTNodeKind::ASTK_BLOCKSTMT) {
      ASTVisitor<NameResolver>::visitBlockStmt(static_cast<BlockStmt *>(Body));
    } else {
      visit(Body);
    }
  }
}

void NameResolver::visitIfStmt(IfStmt *S) {
  S->setScope(ST.getCurrentScope());
  ASTVisitor<NameResolver>::visitIfStmt(S);
}

void NameResolver::visitBlockStmt(BlockStmt *S) {
  ScopedRAII Scoped(ST, ScopeKind::SCOPE_BLOCKSTMT);
  S->setScope(ST.getCurrentScope());
  ASTVisitor<NameResolver>::visitBlockStmt(S);
}

void NameResolver::visitExprStmt(ExprStmt *S) {
  S->setScope(ST.getCurrentScope());
  ASTVisitor<NameResolver>::visitExprStmt(S);
}

void NameResolver::visitFuncDecl(FuncDecl *D) {
  ScopedRAII Scoped(ST, ScopeKind::SCOPE_FUNCTION);
  D->setScope(ST.getCurrentScope());
  for (const auto &Param : D->getParams()) {
    Symbol ParamInfo(SymbolKind::SYMBOL_PARAMETER, false);
    if (!ST.addSymbol(Param.ParamName->getName(), ParamInfo)) {
      Diags.Report(DiagKind::Error, "Parameter already defined",
                   D->getSourceRange().getStart());
    }
    Param.ParamName->setScope(ST.getCurrentScope());
  }
  if (Stmt *Body = D->getBody()) {
    if (Body->getASTNodeKind() == ASTNodeKind::ASTK_BLOCKSTMT) {
      ASTVisitor<NameResolver>::visitBlockStmt(static_cast<BlockStmt *>(Body));
    } else {
      visit(Body);
    }
  }
}

void NameResolver::visitVarExpr(VarExpr *E) {
  E->setScope(ST.getCurrentScope());
  if (!ST.lookupSymbol(E->getName())) {
    Diags.Report(DiagKind::Error, "Undeclared variable",
                 E->getSourceRange().getStart());
  }
}

void NameResolver::visitIntExpr(IntExpr *E) {
  E->setScope(ST.getCurrentScope());
}
void NameResolver::visitFloatExpr(FloatExpr *E) {
  E->setScope(ST.getCurrentScope());
}

void NameResolver::visitRangeExpr(RangeExpr *E) {
  E->setScope(ST.getCurrentScope());
  ASTVisitor<NameResolver>::visitRangeExpr(E);
}

void NameResolver::visitRefrExpr(RefrExpr *E) {
  E->setScope(ST.getCurrentScope());
  ASTVisitor<NameResolver>::visitRefrExpr(E);
}

void NameResolver::visitArrayExpr(ArrayExpr *E) {
  E->setScope(ST.getCurrentScope());
  ASTVisitor<NameResolver>::visitArrayExpr(E);
}

void NameResolver::visitArrayAccessExpr(ArrayAccessExpr *E) {
  E->setScope(ST.getCurrentScope());
  Symbol *Sym = ST.lookupSymbol(E->getArrayNameExpr()->getName());
  E->getArrayNameExpr()->setScope(Sym->getScope());
  for (const auto &Index : E->getIndexVector()) {
    ASTVisitor<NameResolver>::visit(Index.get());
  }
}

void NameResolver::visitFunCall(FunCall *E) {
  if (!ST.lookupSymbol(E->getFuncName()->getName())) {
    Diags.Report(DiagKind::Error, "Undeclared function",
                 E->getSourceRange().getStart());
  }
  for (const auto &Param : E->getParams()) {
    visit(Param.get());
  }
}

void NameResolver::visitReturnStmt(ReturnStmt *S) {
  S->setScope(ST.getCurrentScope());
  ASTVisitor<NameResolver>::visitReturnStmt(S);
}
