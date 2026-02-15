#include "trsc/Sema/SymbolTable.h"
#include "trsc/Sema/Scope.h"
#include <memory>

namespace trsc {

  SymbolTable::SymbolTable() {
    std::unique_ptr<Scope> GlobalScopePtr = std::make_unique<Scope>(ScopeKind::SCOPE_GLOBAL, nullptr, 0);
    GlobalScope = GlobalScopePtr.get();
    CurrentScope = GlobalScope;
    AllScopes.push_back(std::move(GlobalScopePtr));
  }

  void SymbolTable::enterScope(ScopeKind Kind) {
    uint32_t NewDepth = CurrentScope->getDepth() + 1;
    auto NewScopePtr = std::make_unique<Scope>(Kind, CurrentScope, NewDepth);
    CurrentScope = NewScopePtr.get();
    AllScopes.push_back(std::move(NewScopePtr));
  } 

  void SymbolTable::exitScope() {
    if(CurrentScope->getParent()) {
      CurrentScope = CurrentScope->getParent(); 
    }
  }

  bool SymbolTable::addSymbol(const std::string& Name, Symbol Sym) {
    return CurrentScope->addSymbol(Name, Sym);
  }

  Symbol* SymbolTable::lookupSymbol(const std::string& Name) {
    Scope* Current = CurrentScope;
    while(Current) {
      if(Symbol* Sym = Current->lookupSymbolLocal(Name)) {
        return Sym;
      }
      Current = Current->getParent();
    }
    return nullptr;
  }

  Symbol* SymbolTable::lookupSymbol(const std::string& Name, Scope* CurrScope) {
    Scope* Current = CurrScope;
    while(Current) {
      if(Symbol* Sym = Current->lookupSymbolLocal(Name)) {
        return Sym;
      }
      Current = Current->getParent();
    }
    return nullptr;
  }
} // namespace trsc
