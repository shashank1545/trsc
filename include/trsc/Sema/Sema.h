#ifndef TRSC_SEMA_SEMA_H
#define TRSC_SEMA_SEMA_H

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

private:
  DiagnosticsEngine &Diags;
  SymbolTable &ST;
  ASTContext &Ctx;
  DeclarationCollector DeclarationCollector;
  NameResolver NameResolver;
  TypeChecker TypeChecker;
};

} // namespace trsc

#endif // TRSC_SEMA_SEMA_H
