#include "trsc/Sema/DeclarationCollector.h"
#include "trsc/AST/AST.h"

using namespace trsc;

void DeclarationCollector::visitProgram(Program *P) {
  for (const auto& Stmt: P->getStatements()) {
    if (Stmt->getASTNodeKind() == ASTNodeKind::ASTK_FUNCDECL) {
      getDerived().visit(Stmt.get());
    }
  }
}

// TODO: Make the name mangling implementation gloabal for the compiler
// Name Mangling is currently only done for Function names and not fully 
// feldged. C++ naming scheme is used right now , i need to do more 
// research before implementing the rust, till then this will do.
//
void DeclarationCollector::visitFuncDecl(FuncDecl* D) {
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
  if (!ST.addSymbol(D->getFuncName()->getName(), FuncInfo)) {
    Diags.Report(DiagKind::Error, "Function already defined",
        D->getSourceRange().getStart());
  }
}


