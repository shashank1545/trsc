#ifndef TRSC_SEMA_SEMA_H
#define TRSC_SEMA_SEMA_H

#include "trsc/Sema/BorrowChecker.h"
#include "trsc/Sema/DeclarationCollector.h"
#include "trsc/Sema/NameResolver.h"
#include "trsc/Sema/TypeChecker.h"

namespace trsc {

class DiagnosticsEngine;
class SymbolTable;
class ASTContext;

class SemanticAnalyzer {
public:
  SemanticAnalyzer(DiagnosticsEngine &Diags, SymbolTable &ST, ASTContext &Ctx);
  ~SemanticAnalyzer() = default;

  void analyze(ASTNode *Ast);

  SymbolTable &getSymbolTable() { return ST; }
  ASTContext &getASTContext() { return Ctx; }
  DiagnosticsEngine &getDiagnostics() { return Diags; }

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
