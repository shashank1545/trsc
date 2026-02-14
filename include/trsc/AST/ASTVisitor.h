#ifndef TRSC_AST_ASTVISITOR_H
#define TRSC_AST_ASTVISITOR_H

#include "trsc/AST/AST.h"

namespace trsc {

template <typename Derived> 
class ASTVisitor {
public:
  void visit(ASTNode *Node) {
    if (!Node)
      return;

    switch(Node->getASTNodeKind()) {
      case ASTNodeKind::ASTK_PROGRAM:
        getDerived().visitProgram(static_cast<Program*>(Node));
        break;
      case ASTNodeKind::ASTK_TYPENAME:
        getDerived().visitTypeName(static_cast<TypeName*>(Node));
        break;
      case ASTNodeKind::ASTK_BOOLEXPR:
        getDerived().visitBoolExpr(static_cast<BoolExpr*>(Node));
        break;
      case ASTNodeKind::ASTK_VAREXPR:
        getDerived().visitVarExpr(static_cast<VarExpr*>(Node));
        break;
      case ASTNodeKind::ASTK_INTEXPR:
        getDerived().visitIntExpr(static_cast<IntExpr*>(Node));
        break;
      case ASTNodeKind::ASTK_FLOATEXPR:
        getDerived().visitFloatExpr(static_cast<FloatExpr*>(Node));
        break;
      case ASTNodeKind::ASTK_BINEXPR:
        getDerived().visitBinExpr(static_cast<BinExpr*>(Node));
        break;
      case ASTNodeKind::ASTK_RANGEEXPR:
        getDerived().visitRangeExpr(static_cast<RangeExpr*>(Node));
        break;
      case ASTNodeKind::ASTK_LETSTMT:
        getDerived().visitLetStmt(static_cast<LetStmt*>(Node));
        break;
      case ASTNodeKind::ASTK_IFSTMT:
        getDerived().visitIfStmt(static_cast<IfStmt*>(Node));
        break;
      case ASTNodeKind::ASTK_FORSTMT:
        getDerived().visitForStmt(static_cast<ForStmt*>(Node));
        break;
      case ASTNodeKind::ASTK_WHILESTMT:
        getDerived().visitWhileStmt(static_cast<WhileStmt*>(Node));
        break;
      case ASTNodeKind::ASTK_EXPRSTMT:
        getDerived().visitExprStmt(static_cast<ExprStmt*>(Node));
        break;
      case ASTNodeKind::ASTK_BLOCKSTMT:
        getDerived().visitBlockStmt(static_cast<BlockStmt*>(Node));
        break;
      case ASTNodeKind::ASTK_RETURNSTMT:
        getDerived().visitReturnStmt(static_cast<ReturnStmt*>(Node));
        break;
      case ASTNodeKind::ASTK_FUNCALL:
        getDerived().visitFunCall(static_cast<FunCall*>(Node));
        break;
      case ASTNodeKind::ASTK_FUNCDECL:
        getDerived().visitFuncDecl(static_cast<FuncDecl*>(Node));
        break;
      case ASTNodeKind::ASTK_TYPE:
      case ASTNodeKind::ASTK_EXPR:
      case ASTNodeKind::ASTK_NUMEXPR:
      case ASTNodeKind::ASTK_STMT:
        break;
    }
  }

  void visitProgram(Program *P) {
    for (const auto &Stmt : P->getStatements()) {
      getDerived().visit(Stmt.get());
    }
  }

  void visitReturnStmt(ReturnStmt *S) {
    if (S->getReturnValue()) {
      getDerived().visit(S->getReturnValue());
    }
  }

  void visitBlockStmt(BlockStmt *S) {
    for (const auto &Stmt : S->getStatements()) {
      getDerived().visit(Stmt.get());
    }
  }

  void visitLetStmt(LetStmt *S) {
    if (S->getDeclaredVar()) {
      getDerived().visit(S->getDeclaredVar());
    }
    if (S->getInitializer()) {
      getDerived().visit(S->getInitializer());
    }
    if (S->getDeclaredType()) {
      getDerived().visit(S->getDeclaredType());
    }
  }

  void visitIfStmt(IfStmt *S) {
    getDerived().visit(S->getCondition());
    getDerived().visit(S->getThenBranch());
    if (S->getElseBranch()) {
      getDerived().visit(S->getElseBranch());
    }
  }

  void visitForStmt(ForStmt *S) {
    getDerived().visit(S->getInit());
    getDerived().visit(S->getRange());
    getDerived().visit(S->getBody());
  }

  void visitWhileStmt(WhileStmt *S) {
    getDerived().visit(S->getCondition());
    getDerived().visit(S->getBody());
  }

  void visitExprStmt(ExprStmt *S) { 
    getDerived().visit(S->getExpression()); 
  }

  void visitFuncDecl(FuncDecl *D) { 
    getDerived().visit(D->getFuncName());
    for (const auto& Param: D->getParams()) {
      getDerived().visit(Param.ParamName.get());
      getDerived().visit(Param.ParamType.get());
    }
    getDerived().visit(D->getReturnType());
    getDerived().visit(D->getBody());
  }

  void visitBinExpr(BinExpr *E) {
    getDerived().visit(E->getLHS());
    getDerived().visit(E->getRHS());
  }

  void visitRangeExpr(RangeExpr *E) {
    getDerived().visit(E->getStart());
    getDerived().visit(E->getEnd());
  }

  void visitFunCall(FunCall *E) {
    getDerived().visit(E->getFuncName());
    for (const auto &Param : E->getParams()) {
      getDerived().visit(Param.get());
    }
  }

  void visitVarExpr(VarExpr *E) {}
  void visitNumExpr(NumExpr *E) {}
  void visitIntExpr(IntExpr *E) {}
  void visitFloatExpr(FloatExpr *E) {}
  void visitBoolExpr(BoolExpr *E) {}
  void visitTypeName(TypeName *T) {}

protected:
  Derived &getDerived() { return *static_cast<Derived *>(this); }
};
} // namespace trsc

#endif // TRSC_AST_ASTVISITOR_H
