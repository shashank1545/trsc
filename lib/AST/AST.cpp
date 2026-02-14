#include "trsc/AST/AST.h"

namespace trsc {

  const char* getASTKindName(trsc::ASTNodeKind Kind) {
    switch(Kind) {
      case trsc::ASTNodeKind::ASTK_PROGRAM: return "Program"; 
      case trsc::ASTNodeKind::ASTK_TYPE: return "Type";
      case trsc::ASTNodeKind::ASTK_TYPENAME: return "Typename";
      case trsc::ASTNodeKind::ASTK_EXPR: return "Expr";
      case trsc::ASTNodeKind::ASTK_BOOLEXPR: return "BoolExpr";
      case trsc::ASTNodeKind::ASTK_NUMEXPR: return "NumExpr";
      case trsc::ASTNodeKind::ASTK_INTEXPR: return "IntExpr";
      case trsc::ASTNodeKind::ASTK_FLOATEXPR: return "FloatExpr";
      case trsc::ASTNodeKind::ASTK_VAREXPR: return "VarExpr";
      case trsc::ASTNodeKind::ASTK_BINEXPR: return "BinExpr";
      case trsc::ASTNodeKind::ASTK_RANGEEXPR: return "RangeExpr";
      case trsc::ASTNodeKind::ASTK_STMT: return "Stmt";
      case trsc::ASTNodeKind::ASTK_LETSTMT: return "LetStmt";
      case trsc::ASTNodeKind::ASTK_IFSTMT: return "IfStmt";
      case trsc::ASTNodeKind::ASTK_FORSTMT: return "ForStmt";
      case trsc::ASTNodeKind::ASTK_WHILESTMT: return "WhileStmt";
      case trsc::ASTNodeKind::ASTK_EXPRSTMT: return "ExprStmt";
      case trsc::ASTNodeKind::ASTK_BLOCKSTMT: return "BlockStmt";
      case trsc::ASTNodeKind::ASTK_RETURNSTMT: return "ReturnStmt";
      case trsc::ASTNodeKind::ASTK_FUNCALL: return "FunCall";
      case trsc::ASTNodeKind::ASTK_FUNCDECL: return "FuncDecl";
      default: return "Unknown";
    }
  }
} // namespace trsc
