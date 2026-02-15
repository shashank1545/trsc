#include "trsc/Sema/Scope.h"

namespace trsc {
  
  const char* getSymbolKindName(SymbolKind Kind) {
    switch(Kind) {
      case trsc::SymbolKind::SYMBOL_VARIABLE: return "Variable";
      case trsc::SymbolKind::SYMBOL_PARAMETER: return "Parameter";
      case trsc::SymbolKind::SYMBOL_FUNCTION: return "Function";
      case trsc::SymbolKind::SYMBOL_CONST: return "Const";
      default: "Unknown"; 
    };
  } 

  const char* getScopeKindName(ScopeKind Kind) {
    switch (Kind) {
      case trsc::ScopeKind::SCOPE_GLOBAL: return "Global"; 
      case trsc::ScopeKind::SCOPE_FORSTMT: return "Forstmt"; 
      case trsc::ScopeKind::SCOPE_WHILESTMT: return "Whilestmt"; 
      case trsc::ScopeKind::SCOPE_BLOCKSTMT: return "Blockstmt"; 
      case trsc::ScopeKind::SCOPE_FUNCTION: return "Function"; 
      default: "Unknown";
    }
  };
  
bool Scope::addSymbol(const std::string& Name, Symbol Sym) {
    auto const [it, inserted] = Symbols.emplace(Name, Sym);
    return inserted;
}

  Symbol* Scope::lookupSymbolLocal(const std::string& Name) {
    auto it = Symbols.find(Name);
    if(it != Symbols.end()) {
      return &it->second;
    }
    return nullptr;
  }

} // namespace trsc
