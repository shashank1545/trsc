#include "trsc/Sema/DeclarationCollector.h"
#include "trsc/Basic/Diagnostics.h"
#include "trsc/Sema/SymbolTable.h"

using namespace trsc;

void DeclarationCollector::visitProgram(Program *P) {
  for (Stmt *S : P->getStatements()) {
    if (S->getASTNodeKind() == ASTNodeKind::ASTK_FUNCDECL) {
      getDerived().visit(S);
    }
  }
}

// TODO: Make the name mangling implementation global for the compiler
// Name Mangling is currently only done for Function names and not fully
// feldged. C++ naming scheme is used right now , i need to do more
// research before implementing the rust, till then this will do.

void DeclarationCollector::visitFuncDecl(FuncDecl *D) {
  Symbol FuncInfo(SymbolKind::SYMBOL_FUNCTION);
  // std::string Name = D->getFuncName()->getName();
  // if (Name != "main") {
  //   int NameLen = Name.size();
  //   Name = "_Z" + std::to_string(NameLen) + Name;
  // }
  // if(!D->getParams().empty()) {
  //   for (const auto& Param: D->getParams()) {
  //     Name = Name + Param.ParamType->getName()[0];
  //   }
  // }
  FuncInfo.setScope(ST.getCurrentScope());
  if (Symbol *Declared =
          ST.addSymbol(D->getFuncName()->getIdentifierInfo(), FuncInfo)) {
    D->getFuncName()->setScope(ST.getCurrentScope());
    D->getFuncName()->setSymbol(Declared);
  } else {
    Diags.Report(DiagKind::Error, "Function already defined",
                 D->getSourceRange().getStart());
  }
}
