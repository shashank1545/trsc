#include "trsc/Sema/Sema.h"

using namespace trsc; 

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

