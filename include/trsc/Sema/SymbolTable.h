#ifndef TRSC_SEMA_SYMBOLTABLE_H
#define TRSC_SEMA_SYMBOLTABLE_H

#include <deque>
#include <memory>

#include "trsc/Basic/IdentifierTable.h"
#include "trsc/Sema/Scope.h"

namespace trsc {

class SymbolTable {
private:
  std::vector<std::unique_ptr<Scope>> AllScopes;
  // Backing store for every Symbol. std::deque never invalidates references on
  // push_back, so the Symbol * cached on AST nodes in NameResolver stays valid
  // through MLIRGen, which mutates Symbol::Op in place.
  std::deque<Symbol> SymbolArena;
  IdentifierTable &Idents;
  Scope *CurrentScope;
  Scope *GlobalScope;

public:
  explicit SymbolTable(IdentifierTable &Idents);

  void enterScope(ScopeKind Kind);
  void exitScope();

  // Symbol Operation searches from local to global. addSymbol returns the
  // interned Symbol *, or nullptr if Name is already declared in this scope.
  Symbol *addSymbol(const IdentifierInfo *Name, Symbol Sym);
  Symbol *lookupSymbol(const IdentifierInfo *Name);
  Symbol *lookupSymbol(const IdentifierInfo *Name, Scope *Scope);

  // Spelling-keyed overloads for callers that hold only a name. They intern
  // once and then take the pointer-keyed path above; prefer the overloads
  // taking an IdentifierInfo * wherever one is already at hand.
  Symbol *addSymbol(const std::string &Name, Symbol Sym);
  Symbol *lookupSymbol(const std::string &Name);
  Symbol *lookupSymbol(const std::string &Name, Scope *Scope);

  Scope *getCurrentScope() const { return CurrentScope; }
  Scope *getGlobalScope() const { return GlobalScope; }

  const std::vector<std::unique_ptr<Scope>> &getAllScopes() const {
    return AllScopes;
  }
};

class ScopedRAII {
private:
  SymbolTable &Table;

public:
  ScopedRAII(SymbolTable &ST, ScopeKind Kind) : Table(ST) {
    Table.enterScope(Kind);
  }

  ~ScopedRAII() { Table.exitScope(); }

  ScopedRAII(const ScopedRAII &) = delete;
  ScopedRAII &operator=(const ScopedRAII &) = delete;
};

} // namespace trsc

#endif // TRSC_SEMA_SYMBOLTABLE_H
