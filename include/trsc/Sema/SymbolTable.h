#ifndef TRSC_SEMA_SYMBOLTABLE_H
#define TRSC_SEMA_SYMBOLTABLE_H

#include <memory>
#include <vector>

#include "trsc/Sema/Scope.h"

namespace trsc {

  class SymbolTable {
    private:
      std::vector<std::unique_ptr<Scope>> AllScopes;
      Scope* CurrentScope;
      Scope* GlobalScope;

    public:
      SymbolTable();

      void enterScope(ScopeKind Kind);
      void exitScope();

      // Symbol Operation searches from local to global
      bool addSymbol(const std::string& Name, Symbol Sym);
      Symbol* lookupSymbol(const std::string& Name);
      Symbol* lookupSymbol(const std::string& Name, Scope* Scope);
      
      Scope* getCurrentScope() const {return CurrentScope;}
      Scope* getGlobalScope() const {return GlobalScope;}

      const std::vector<std::unique_ptr<Scope>>& getAllScopes() const {
        return AllScopes;
      }
  };

  class ScopedRAII {
    private:
      SymbolTable& Table;

    public:
      ScopedRAII(SymbolTable& ST, ScopeKind Kind): Table(ST) {
        Table.enterScope(Kind);
      }

      ~ScopedRAII() {Table.exitScope();}

      ScopedRAII(const ScopedRAII&) = delete;
      ScopedRAII& operator=(const ScopedRAII&) = delete;
  };

} // namespace trsc

#endif // TRSC_SEMA_SYMBOLTABLE_H
