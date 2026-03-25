#include "trsc/AST/TypedASTPrinter.h"
#include "trsc/Lex/Token.h"
#include <iomanip>

namespace trsc {

  TypedASTPrinter::TypedASTPrinter(std::ostream &OS) : OS(OS), IndentLevel(0) {
    IsLastStack.clear();
  }

  void TypedASTPrinter::printIndent(bool isLast) {
    for (unsigned i = 0; i < IndentLevel; ++i) {
      if (i < IsLastStack.size() && !IsLastStack[i]) {
        OS << "│   ";
      } else {
        OS << "    ";
      }
    }
    if (IndentLevel > 0) {
      OS << (isLast ? "└── " : "├── ");
    }
  }

  std::string TypedASTPrinter::getAddressString(const void *Ptr) {
    std::ostringstream oss;
    oss << "0x" << std::hex << reinterpret_cast<uintptr_t>(Ptr);
    return oss.str();
  }

  std::string TypedASTPrinter::getLocationString(const ASTNode *Node) {
    SourceRange Range = Node->getSourceRange();
    std::ostringstream oss;

    if (Range.isValid()) {
      oss << "<" << Range.getStart().asString();
    }
    if (!(Range.getStart() == Range.getEnd())) {
      oss << ", " << Range.getEnd().asString();
    }
    oss << ">";

    return oss.str();
  }

  std::string TypedASTPrinter::getTypeString(const Expr *E) {
    QualType Type = E->getType();
    if (Type.isNull()) {
      return "";
    }

    std::ostringstream oss;
    oss << " '" << Type.getAsString() << "'";
    return oss.str();
  }

  void TypedASTPrinter::printNodeHeader(const ASTNode *Node, const std::string &NodeName) {
    OS << NodeName << " " << getAddressString(Node) << " " << getLocationString(Node);
  }


  void TypedASTPrinter::visitProgram(Program *Node) {
    printNodeHeader(Node, "Program");
    OS << "\n";

    const auto &Statements = Node->getStatements();
    IndentLevel++;

    for (size_t i = 0; i < Statements.size(); ++i) {
      bool isLast = (i == Statements.size() - 1);
      if (IndentLevel > 0) {
        IsLastStack.resize(IndentLevel - 1);
        IsLastStack.push_back(isLast);
      }

      printIndent(isLast);
      visit(Statements[i].get());

      if (IndentLevel > 0 && !IsLastStack.empty()) {
        IsLastStack.pop_back();
      }
    }

    IndentLevel--;
  }

  void TypedASTPrinter::visitLetStmt(LetStmt *Node) {
    printNodeHeader(Node, "LetStmt");
    OS << " " << (Node->isMut() ? "mut" : "immut");
    OS << "\n";

    IndentLevel++;

    // Print variable
    if (Node->getDeclaredVar()) {
      bool hasMore = Node->getDeclaredType() || Node->getInitializer();
      if (IndentLevel > 0) {
        IsLastStack.resize(IndentLevel - 1);
        IsLastStack.push_back(!hasMore);
      }
      printIndent(!hasMore);
      visit(Node->getDeclaredVar());
      if (IndentLevel > 0 && !IsLastStack.empty()) {
        IsLastStack.pop_back();
      }
    }

    // Print type
    if (Node->getDeclaredType()) {
      bool hasMore = Node->getInitializer() != nullptr;
      if (IndentLevel > 0) {
        IsLastStack.resize(IndentLevel - 1);
        IsLastStack.push_back(!hasMore);
      }
      printIndent(!hasMore);
      visit(Node->getDeclaredType());
      if (IndentLevel > 0 && !IsLastStack.empty()) {
        IsLastStack.pop_back();
      }
    }

    // Print initializer
    if (Node->getInitializer()) {
      if (IndentLevel > 0) {
        IsLastStack.resize(IndentLevel - 1);
        IsLastStack.push_back(true);
      }
      printIndent(true);
      visit(Node->getInitializer());
      if (IndentLevel > 0 && !IsLastStack.empty()) {
        IsLastStack.pop_back();
      }
    }

    IndentLevel--;
  }

  void TypedASTPrinter::visitIfStmt(IfStmt *Node) {
    printNodeHeader(Node, "IfStmt");
    OS << "\n";

    IndentLevel++;

    // Condition
    if (Node->getCondition()) {
      bool hasMore = Node->getThenBranch() || Node->getElseBranch();
      if (IndentLevel > 0) {
        IsLastStack.resize(IndentLevel - 1);
        IsLastStack.push_back(!hasMore);
      }
      printIndent(!hasMore);
      visit(Node->getCondition());
      if (IndentLevel > 0 && !IsLastStack.empty()) {
        IsLastStack.pop_back();
      }
    }

    // Then branch
    if (Node->getThenBranch()) {
      bool hasMore = Node->getElseBranch() != nullptr;
      if (IndentLevel > 0) {
        IsLastStack.resize(IndentLevel - 1);
        IsLastStack.push_back(!hasMore);
      }
      printIndent(!hasMore);
      visit(Node->getThenBranch());
      if (IndentLevel > 0 && !IsLastStack.empty()) {
        IsLastStack.pop_back();
      }
    }

    // Else branch
    if (Node->getElseBranch()) {
      if (IndentLevel > 0) {
        IsLastStack.resize(IndentLevel - 1);
        IsLastStack.push_back(true);
      }
      printIndent(true);
      visit(Node->getElseBranch());
      if (IndentLevel > 0 && !IsLastStack.empty()) {
        IsLastStack.pop_back();
      }
    }

    IndentLevel--;
  }

  void TypedASTPrinter::visitWhileStmt(WhileStmt *Node) {
    printNodeHeader(Node, "WhileStmt");
    OS << "\n";

    IndentLevel++;

    // Condition
    if (Node->getCondition()) {
      if (IndentLevel > 0) {
        IsLastStack.resize(IndentLevel - 1);
        IsLastStack.push_back(false);
      }
      printIndent(false);
      visit(Node->getCondition());
      if (IndentLevel > 0 && !IsLastStack.empty()) {
        IsLastStack.pop_back();
      }
    }

    // Body
    if (Node->getBody()) {
      if (IndentLevel > 0) {
        IsLastStack.resize(IndentLevel - 1);
        IsLastStack.push_back(true);
      }
      printIndent(true);
      visit(Node->getBody());
      if (IndentLevel > 0 && !IsLastStack.empty()) {
        IsLastStack.pop_back();
      }
    }

    IndentLevel--;
  }

  void TypedASTPrinter::visitForStmt(ForStmt *Node) {
    printNodeHeader(Node, "ForStmt");
    OS << "\n";

    IndentLevel++;

    // Init variable
    if (Node->getInit()) {
      if (IndentLevel > 0) {
        IsLastStack.resize(IndentLevel - 1);
        IsLastStack.push_back(false);
      }
      printIndent(false);
      visit(Node->getInit());
      if (IndentLevel > 0 && !IsLastStack.empty()) {
        IsLastStack.pop_back();
      }
    }

    // Range
    if (Node->getRange()) {
      bool hasMore = Node->getBody() != nullptr;
      if (IndentLevel > 0) {
        IsLastStack.resize(IndentLevel - 1);
        IsLastStack.push_back(!hasMore);
      }
      printIndent(!hasMore);
      visit(Node->getRange());
      if (IndentLevel > 0 && !IsLastStack.empty()) {
        IsLastStack.pop_back();
      }
    }

    // Body
    if (Node->getBody()) {
      if (IndentLevel > 0) {
        IsLastStack.resize(IndentLevel - 1);
        IsLastStack.push_back(true);
      }
      printIndent(true);
      visit(Node->getBody());
      if (IndentLevel > 0 && !IsLastStack.empty()) {
        IsLastStack.pop_back();
      }
    }

    IndentLevel--;
  }

  void TypedASTPrinter::visitBlockStmt(BlockStmt *Node) {
    printNodeHeader(Node, "BlockStmt");
    OS << "\n";

    const auto &Statements = Node->getStatements();
    IndentLevel++;

    for (size_t i = 0; i < Statements.size(); ++i) {
      bool isLast = (i == Statements.size() - 1);
      if (IndentLevel > 0) {
        IsLastStack.resize(IndentLevel - 1);
        IsLastStack.push_back(isLast);
      }

      printIndent(isLast);
      visit(Statements[i].get());

      if (IndentLevel > 0 && !IsLastStack.empty()) {
        IsLastStack.pop_back();
      }
    }

    IndentLevel--;
  }

  void TypedASTPrinter::visitExprStmt(ExprStmt *Node) {
    printNodeHeader(Node, "ExprStmt");
    OS << "\n";

    if (Node->getExpression()) {
      IndentLevel++;
      if (IndentLevel > 0) {
        IsLastStack.resize(IndentLevel - 1);
        IsLastStack.push_back(true);
      }
      printIndent(true);
      visit(Node->getExpression());
      if (IndentLevel > 0 && !IsLastStack.empty()) {
        IsLastStack.pop_back();
      }
      IndentLevel--;
    }
  }

  void TypedASTPrinter::visitReturnStmt(ReturnStmt *Node) {
    printNodeHeader(Node, "ReturnStmt");
    OS << "\n";

    if (Node->getReturnValue()) {
      IndentLevel++;
      if (IndentLevel > 0) {
        IsLastStack.resize(IndentLevel - 1);
        IsLastStack.push_back(true);
      }
      printIndent(true);
      visit(Node->getReturnValue());
      if (IndentLevel > 0 && !IsLastStack.empty()) {
        IsLastStack.pop_back();
      }
      IndentLevel--;
    }
  }

  void TypedASTPrinter::visitFuncDecl(FuncDecl *Node) {
    printNodeHeader(Node, "FuncDecl");
    if (Node->getFuncName()) {
      OS << " '" << Node->getFuncName()->getName() << "'";
    }
    OS << "\n";

    IndentLevel++;

    const auto &Params = Node->getParams();
    bool hasBody = Node->getBody() != nullptr;
    bool hasReturn = Node->getReturnType() != nullptr;

    // Print parameters
    for (size_t i = 0; i < Params.size(); ++i) {
      bool isLast = (i == Params.size() - 1) && !hasReturn && !hasBody;
      if (IndentLevel > 0) {
        IsLastStack.resize(IndentLevel - 1);
        IsLastStack.push_back(isLast);
      }

      printIndent(isLast);
      OS << "Param " << getAddressString(&Params[i]) << "\n";

      IndentLevel++;

      if (Params[i].ParamName) {
        if (IndentLevel > 0) {
          IsLastStack.resize(IndentLevel - 1);
          IsLastStack.push_back(false);
        }
        printIndent(false);
        visit(Params[i].ParamName.get());
        if (IndentLevel > 0 && !IsLastStack.empty()) {
          IsLastStack.pop_back();
        }
      }

      if (Params[i].ParamType) {
        if (IndentLevel > 0) {
          IsLastStack.resize(IndentLevel - 1);
          IsLastStack.push_back(true);
        }
        printIndent(true);
        visit(Params[i].ParamType.get());
        if (IndentLevel > 0 && !IsLastStack.empty()) {
          IsLastStack.pop_back();
        }
      }

      IndentLevel--;

      if (IndentLevel > 0 && !IsLastStack.empty()) {
        IsLastStack.pop_back();
      }
    }

    // Print return type
    if (Node->getReturnType()) {
      bool isLast = !hasBody;
      if (IndentLevel > 0) {
        IsLastStack.resize(IndentLevel - 1);
        IsLastStack.push_back(isLast);
      }
      printIndent(isLast);
      visit(Node->getReturnType());
      if (IndentLevel > 0 && !IsLastStack.empty()) {
        IsLastStack.pop_back();
      }
    }

    // Print body
    if (Node->getBody()) {
      if (IndentLevel > 0) {
        IsLastStack.resize(IndentLevel - 1);
        IsLastStack.push_back(true);
      }
      printIndent(true);
      visit(Node->getBody());
      if (IndentLevel > 0 && !IsLastStack.empty()) {
        IsLastStack.pop_back();
      }
    }

    IndentLevel--;
  }

  void TypedASTPrinter::visitFunCall(FunCall *Node) {
    printNodeHeader(Node, "FunCall");
    if (Node->getFuncName()) {
      OS << " '" << Node->getFuncName()->getName() << "'";
    }
    OS << getTypeString(Node);
    OS << "\n";

    const auto &Params = Node->getParams();
    if (!Params.empty()) {
      IndentLevel++;

      for (size_t i = 0; i < Params.size(); ++i) {
        bool isLast = (i == Params.size() - 1);
        if (IndentLevel > 0) {
          IsLastStack.resize(IndentLevel - 1);
          IsLastStack.push_back(isLast);
        }

        printIndent(isLast);
        visit(Params[i].get());

        if (IndentLevel > 0 && !IsLastStack.empty()) {
          IsLastStack.pop_back();
        }
      }

      IndentLevel--;
    }
  }

  void TypedASTPrinter::visitIntExpr(IntExpr *Node) {
    printNodeHeader(Node, "IntExpr");
    OS << getTypeString(Node);
    OS << " " << Node->getValue();
    OS << "\n";
  }

  void TypedASTPrinter::visitFloatExpr(FloatExpr *Node) {
    printNodeHeader(Node, "FloatExpr");
    OS << getTypeString(Node);
    OS << " " << Node->getValue();
    OS << "\n";
  }

  void TypedASTPrinter::visitVarExpr(VarExpr *Node) {
    printNodeHeader(Node, "VarExpr");
    OS << getTypeString(Node);
    OS << " '" << Node->getName() << "'";
    OS << "\n";
  }

  void TypedASTPrinter::visitASExpr(ASExpr *Node) {
    printNodeHeader(Node, "ASExpr");
    OS << getTypeString(Node);
    OS << "\n";

    IndentLevel++;

    if (Node->getFromExpr()) {
      bool hasMore = Node->getToType() != nullptr;
      if (IndentLevel > 0) {
        IsLastStack.resize(IndentLevel - 1);
        IsLastStack.push_back(!hasMore);
      }
      printIndent(!hasMore);
      visit(Node->getFromExpr());
      if (IndentLevel > 0 && !IsLastStack.empty()) {
        IsLastStack.pop_back();
      }
    }

    if (Node->getToType()) {
      if (IndentLevel > 0) {
        IsLastStack.resize(IndentLevel - 1);
        IsLastStack.push_back(true);
      }
      printIndent(true);
      visit(Node->getToType());
      if (IndentLevel > 0 && !IsLastStack.empty()) {
        IsLastStack.pop_back();
      }
    }

    IndentLevel--;
  }

  void TypedASTPrinter::visitRefrExpr(RefrExpr *Node) {
    printNodeHeader(Node, "RefrExpr");
    OS << getTypeString(Node);
    OS << " " << (Node->isMut() ? "mut" : "immut");
    OS << "\n";

    if (Node->getReferent()) {
      IndentLevel++;
      if (IndentLevel > 0) {
        IsLastStack.resize(IndentLevel - 1);
        IsLastStack.push_back(true);  
      }
      printIndent(true);
      visit(Node->getReferent());
      if (IndentLevel > 0 && !IsLastStack.empty()) {
        IsLastStack.pop_back();
      }
      IndentLevel--;
    }
  }

  void TypedASTPrinter::visitBoolExpr(BoolExpr *Node) {
    printNodeHeader(Node, "BoolExpr");
    OS << getTypeString(Node);
    OS << " " << (Node->getValue() ? "true" : "false");
    OS << "\n";
  }

  void TypedASTPrinter::visitBinExpr(BinExpr *Node) {
    printNodeHeader(Node, "BinExpr");
    OS << getTypeString(Node);
    OS << " '" << Lex::getTokenName(Node->getOp()) << "'";
    OS << "\n";

    IndentLevel++;

    // Left operand
    if (Node->getLHS()) {
      if (IndentLevel > 0) {
        IsLastStack.resize(IndentLevel - 1);
        IsLastStack.push_back(false);
      }
      printIndent(false);
      visit(Node->getLHS());
      if (IndentLevel > 0 && !IsLastStack.empty()) {
        IsLastStack.pop_back();
      }
    }

    // Right operand
    if (Node->getRHS()) {
      if (IndentLevel > 0) {
        IsLastStack.resize(IndentLevel - 1);
        IsLastStack.push_back(true);
      }
      printIndent(true);
      visit(Node->getRHS());
      if (IndentLevel > 0 && !IsLastStack.empty()) {
        IsLastStack.pop_back();
      }
    }

    IndentLevel--;
  }


  void TypedASTPrinter::visitRangeExpr(RangeExpr *Node) {
    printNodeHeader(Node, "RangeExpr");
    OS << getTypeString(Node);
    OS << " " << (Node->isInclusive() ? "inclusive" : "exclusive");
    OS << "\n";

    IndentLevel++;

    // Start
    if (Node->getStart()) {
      if (IndentLevel > 0) {
        IsLastStack.resize(IndentLevel - 1);
        IsLastStack.push_back(false);
      }
      printIndent(false);
      visit(Node->getStart());
      if (IndentLevel > 0 && !IsLastStack.empty()) {
        IsLastStack.pop_back();
      }
    }

    // End
    if (Node->getEnd()) {
      if (IndentLevel > 0) {
        IsLastStack.resize(IndentLevel - 1);
        IsLastStack.push_back(true);
      }
      printIndent(true);
      visit(Node->getEnd());
      if (IndentLevel > 0 && !IsLastStack.empty()) {
        IsLastStack.pop_back();
      }
    }

    IndentLevel--;
  }

  void TypedASTPrinter::visitTypeName(TypeName *Node) {
    printNodeHeader(Node, "TypeName");
    OS << " '" << Node->getName() << "'";
    OS << "\n";
  }

  void TypedASTPrinter::visitPointerTypeName(PointerTypeName *Node) {
    printNodeHeader(Node, "PointerTypeName");
    OS << " '" << Node->getName() << "'";  
    OS << "\n";
  }
  void TypedASTPrinter::visitReferenceTypeName(ReferenceTypeName *Node) {
    printNodeHeader(Node, "ReferenceTypeName");
    OS << " '" << Node->getName() << "'";
    OS << "\n";
  }
  void TypedASTPrinter::visitArrayTypeName(ArrayTypeName *Node) {
    printNodeHeader(Node, "ArrayTypeName");
    OS << " '" << Node->getName() << "'";
    OS << "\n";
  }

} // namespace trsc
