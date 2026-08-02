#include "trsc/Sema/SymbolTable.h"
#include "trsc/Sema/Scope.h"
#include <memory>

namespace trsc {

SymbolTable::SymbolTable(IdentifierTable &Idents) : Idents(Idents) {
  std::unique_ptr<Scope> GlobalScopePtr =
      std::make_unique<Scope>(ScopeKind::SCOPE_GLOBAL, nullptr, 0);
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
  if (CurrentScope->getParent()) {
    CurrentScope = CurrentScope->getParent();
  }
}

Symbol *SymbolTable::addSymbol(const IdentifierInfo *Name, Symbol Sym) {
  // Single redefinition probe lives here, before touching the arena so a
  // rejected insert does not leave an orphaned Symbol behind;
  // addSymbolUnchecked skips Scope's own duplicate check.
  if (CurrentScope->lookupSymbolLocal(Name))
    return nullptr;
  Sym.Name = Name;
  SymbolArena.push_back(Sym);
  return CurrentScope->addSymbolUnchecked(Name, &SymbolArena.back());
}

Symbol *SymbolTable::lookupSymbol(const IdentifierInfo *Name) {
  return lookupSymbol(Name, CurrentScope);
}

Symbol *SymbolTable::lookupSymbol(const IdentifierInfo *Name,
                                  Scope *CurrScope) {
  Scope *Current = CurrScope;
  while (Current) {
    if (Symbol *Sym = Current->lookupSymbolLocal(Name)) {
      return Sym;
    }
    Current = Current->getParent();
  }
  return nullptr;
}

Symbol *SymbolTable::addSymbol(const std::string &Name, Symbol Sym) {
  return addSymbol(Idents.get(Name), Sym);
}

Symbol *SymbolTable::lookupSymbol(const std::string &Name) {
  return lookupSymbol(Name, CurrentScope);
}

Symbol *SymbolTable::lookupSymbol(const std::string &Name, Scope *CurrScope) {
  // find() rather than get(): a lookup must not intern a spelling that was
  // never declared.
  if (const IdentifierInfo *Id = Idents.find(Name))
    return lookupSymbol(Id, CurrScope);
  return nullptr;
}
} // namespace trsc
