#include "trsc/Sema/BorrowChecker.h"
#include "trsc/AST/AST.h"

namespace trsc {

  BorrowChecker::BorrowChecker(DiagnosticsEngine &Diags, SymbolTable &ST, 
      ASTContext &Ctx) : Diags(Diags), ST(ST), Ctx(Ctx) {}

  void BorrowChecker::visitLetStmt(LetStmt *Node) {
    if (Node->getInitializer()) {
      visit(Node->getInitializer());
    }
  }

  void BorrowChecker::visitVarExpr(VarExpr *Node) {
    if (!ST.lookupSymbol(Node->getName(), Node->getScope())) {
      Diags.Report(DiagKind::Error,
          "BorrowChecker: Undeclared variable '" + Node->getName() + "'",
          Node->getSourceRange().getStart());
    }
  }

  void BorrowChecker::visitBinExpr(BinExpr *Node) {
    if (Node->getLHS()) {
      visit(Node->getLHS());
    }
    if (Node->getRHS()) {
      visit(Node->getRHS());
    }
  }

  void BorrowChecker::visitIfStmt(IfStmt *Node) {
    if (Node->getCondition()) {
      visit(Node->getCondition());
    }
    if (Node->getThenBranch()) {
      visit(Node->getThenBranch());
    }
    if (Node->getElseBranch()) {
      visit(Node->getElseBranch());
    }
  }

  void BorrowChecker::visitWhileStmt(WhileStmt *Node) {
    if (Node->getCondition()) {
      visit(Node->getCondition());
    }
    if (Node->getBody()) {
      visit(Node->getBody());
    }
  }

  void BorrowChecker::visitFuncDecl(FuncDecl *Node) {
    for (const auto &Param : Node->getParams()) {
      auto Symbol = ST.lookupSymbol(Param.ParamName->getName(), Node->getScope());
      if (!Symbol) {
        Diags.Report(DiagKind::Error,
            "BorrowChecker: Parameter '" + Param.ParamName->getName() + 
            "' not found in symbol table",
            Node->getSourceRange().getStart());
      }
    }
    if (Node->getBody()) {
      visit(Node->getBody());
    }
  }

  void BorrowChecker::visitFunCall(FunCall *Node) {
    if(!Node->getParams().empty()) {
      for(const auto& Param: Node->getParams()) {
        visit(Param.get());
      }
    }
  }

  void BorrowChecker::visitReturnStmt(ReturnStmt *node) {
    if (node->getReturnValue()) {
      visit(node->getReturnValue());
    }
  }

} // namespace trsc
