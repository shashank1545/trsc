#ifndef TRSC_SEMA_DECLARATIONCOLLECTER_H
#define TRSC_SEMA_DECLARATIONCOLLECTER_H

#include "trsc/AST/AST.h"
#include "trsc/AST/ASTVisitor.h"
#include "trsc/Basic/Diagnostics.h"
#include "trsc/Sema/SymbolTable.h"

namespace trsc {

  // This is the first pass into Semaa.
  // For now it just collects all the function delecaration and adds that 
  // to the SymbolTable for name resolution. It could be done in the 
  // NameResolver class but if i have to add two passes i want them to be 
  // seperate and want them to have different jobs.

  class DeclarationCollector: public ASTVisitor<DeclarationCollector> {
    protected:
      SymbolTable &ST;
      DiagnosticsEngine &Diags;
    public:
      DeclarationCollector(DiagnosticsEngine &Diags, SymbolTable &ST): ST(ST),
      Diags(Diags) {}
      
    void visitProgram(Program *P);
    void visitFuncDecl(FuncDecl* D);

  };
}
#endif // TRSC_SEMA_DECLARATIONCOLLECTER_H
