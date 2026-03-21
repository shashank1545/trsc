// TODO :: Fix this after completing Sema.
#include "trsc/AST/AST.h"
#include "trsc/Lex/Token.h"
#include "trsc/AST/ASTPrinter.h"

namespace trsc {

  void ASTPrinter::indent() {
    for (unsigned i = 0; i < IndentLevel; ++i) {
      OS << "  ";
    }
  }

  void ASTPrinter::visitProgram(Program *Node) {
    OS << "Program\n";
    IndentLevel++;
    for (auto &S : Node->getStatements()) {
      if (S) {
        visit(S.get());
      }
    }
  }

  void ASTPrinter::visitLetStmt(LetStmt *Node) {
    indent();
    OS << "LetStmt\n";
    IndentLevel++;
    if (Node->getDeclaredVar()) {
      visit(Node->getDeclaredVar());
    }
    if (Node->getDeclaredType()) {
      visit(Node->getDeclaredType());
    }
    if (Node->getInitializer()) {
      visit(Node->getInitializer());
    }
    IndentLevel--;
  }

  void ASTPrinter::visitIfStmt(IfStmt *Node) {
    indent();
    OS << "IfStmt: ";
    IndentLevel++;
    indent();
    OS << "Condition: ";
    IndentLevel++;
    visit(Node->getCondition());
    IndentLevel--;

    indent();
    OS << "Then: ";
    IndentLevel++;
    visit(Node->getThenBranch());
    IndentLevel--;

    if (Node->getElseBranch()) {
      indent();
      OS << "Else:\n";
      IndentLevel++;
      visit(Node->getElseBranch());
      IndentLevel--;
    }
    IndentLevel--;
  }

  void ASTPrinter::visitWhileStmt(WhileStmt *Node) {
    indent();
    OS << "WhileStmt\n";
    IndentLevel++;
    indent();
    OS << "Condition:\n";
    IndentLevel++;
    visit(Node->getCondition());
    IndentLevel--;

    indent();
    OS << "Body:\n";
    IndentLevel++;
    visit(Node->getBody());
    IndentLevel--;
    IndentLevel--;
  }

  void ASTPrinter::visitForStmt(ForStmt *Node) {
    indent();
    OS << "ForStmt: ";
    IndentLevel++;
    if (Node->getInit()) {
      OS << "Init: ";
      visit(Node->getInit());
    }
    if (Node->getRange()) {
      indent();
      OS << "\nRange: ";
      IndentLevel++;
      visit(Node->getRange());
      IndentLevel--;
    }
    indent();
    OS << "Body: ";
    IndentLevel++;
    visit(Node->getBody());
    IndentLevel--;
    IndentLevel--;
  }

  void ASTPrinter::visitBlockStmt(BlockStmt *Node) {
    IndentLevel++;
    for (auto &S : Node->getStatements()) {
      if (S) {
        visit(S.get());
      }
    }
    IndentLevel--;
  }

  void ASTPrinter::visitExprStmt(ExprStmt *Node) {
    indent();
    OS << "ExprStmt\n";
    IndentLevel++;
    visit(Node->getExpression());
    IndentLevel--;
  }

  void ASTPrinter::visitIntExpr(IntExpr *Node) {
    indent();
    OS << "IntExpr: " << Node->getValue() << "\n";
  }

  void ASTPrinter::visitFloatExpr(FloatExpr *Node) {
    indent();
    OS << "FloatExpr: " << Node->getValue() << "\n";
  }

  void ASTPrinter::visitVarExpr(VarExpr *Node) {
    indent();
    OS << "VarExpr: '" << Node->getName() << "'\n";
  }

  void ASTPrinter::visitBoolExpr(BoolExpr *Node) {
    indent();
    OS << "BoolExpr: " << (Node->getValue() ? "true" : "false") << "\n";
  }

  void ASTPrinter::visitBinExpr(BinExpr *Node) {
    indent();
    OS << "BinExpr: '" << Lex::getTokenName(Node->getOp()) << "'\n";
    IndentLevel++;
    visit(Node->getLHS());
    visit(Node->getRHS());
    IndentLevel--;
  }

  void ASTPrinter::visitTypeName(TypeName *Node) {
    indent();
    OS << "TypeName: '" << Node->getName() << "'\n";
  }

  void ASTPrinter::visitFuncDecl(FuncDecl *Node) {
    indent();
    OS << "FuncDecl '" << Node->getFuncName()->getName() << "'\n";
    IndentLevel++;
    if (Node->getBody()) {
      BlockStmt* BodyBlock = static_cast<BlockStmt*>(Node->getBody());
      for (auto &S : BodyBlock->getStatements()) {
        if (S) {
          visit(S.get());
        }
      }
    }
    IndentLevel--;
  }

  void ASTPrinter::visitFunCall(FunCall *Node) {
    indent();
    OS << "FunCall" << Node->getFuncName()->getName() << "\n";
    if(Node->getParams().size() != 0) {
      for(auto &E: Node->getParams()) {
        if (E) {
          visit(E.get());
        }
      }
    }
    IndentLevel--;
  }

  void ASTPrinter::visitRefrExpr(RefrExpr *Node) {
  indent();
  OS << "RefrExpr " << (Node->isMut() ? "mut" : "non-mut") << '\n';
  IndentLevel++;
  visit(Node->getRefrend());
  IndentLevel--;
  }

  void ASTPrinter::visitRangeExpr(RangeExpr *Node) {
    indent();
    OS << "RangeExpr " << (Node->isInclusive() ? true : false)<< "\n";
    IndentLevel++;
    visit(Node->getStart());
    visit(Node->getEnd());
    IndentLevel--;
  }

  void ASTPrinter::visitReturnStmt(ReturnStmt *Node) {
    indent();
    OS << "ReturnStmt " << '\n';
    visit(Node->getReturnValue());
  }

} // namespace trsc
