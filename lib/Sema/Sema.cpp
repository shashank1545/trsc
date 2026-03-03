#include "trsc/Sema/Sema.h"
#include "trsc/AST/ASTContext.h"
#include "trsc/Sema/DeclarationCollector.h"
#include "trsc/Sema/SymbolTable.h"
#include "trsc/Sema/TypeChecker.h"
#include "trsc/Sema/BorrowChecker.h" 

namespace trsc {

  SemanticAnalyzer::SemanticAnalyzer(DiagnosticsEngine &Diags, SymbolTable &ST, 
      ASTContext &Ctx) : Diags(Diags), ST(ST), Ctx(Ctx), 
  DeclarationCollector(Diags, ST), 
  NameResolver(Diags, ST), TypeChecker(Diags, ST, Ctx), 
  BorrowChecker(Diags, ST, Ctx) {}

  SemanticAnalyzer::~SemanticAnalyzer() = default; 

  void SemanticAnalyzer::analyze(ASTNode *Ast) {
    DeclarationCollector.visit(Ast);
    NameResolver.visit(Ast);
    TypeChecker.visit(Ast);
    BorrowChecker.visit(Ast);
  }

} // namespace trsc
