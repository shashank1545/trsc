#ifndef TRSC_SEMA_SEMA_H
#define TRSC_SEMA_SEMA_H

#include "trsc/AST/AST.h"
#include "trsc/AST/ASTContext.h"
#include "trsc/Basic/Diagnostics.h"
#include "trsc/Sema/BorrowChecker.h"
#include "trsc/Sema/DeclarationCollector.h"
#include "trsc/Sema/NameResolver.h"
#include "trsc/Sema/SymbolTable.h"
#include "trsc/Sema/TypeChecker.h"

namespace trsc {

class SemanticAnalyzer {
public:
  SemanticAnalyzer(DiagnosticsEngine &Diags, SymbolTable &ST, ASTContext &Ctx); 
  ~SemanticAnalyzer(); 

  void analyze(ASTNode *Ast);

  SymbolTable& getSymbolTable() { return ST; }
  ASTContext& getASTContext() { return Ctx; }
  DiagnosticsEngine& getDiagnostics() { return Diags; }

private:
  DiagnosticsEngine &Diags;
  SymbolTable &ST;
  ASTContext &Ctx;
  DeclarationCollector DeclarationCollector;
  NameResolver NameResolver;
  TypeChecker TypeChecker;
  BorrowChecker BorrowChecker;
};

} // namespace trsc

#endif // TRSC_SEMA_SEMA_H
