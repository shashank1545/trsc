#ifndef TRSC_SEMA_SCOPE_H
#define TRSC_SEMA_SCOPE_H

#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "trsc/AST/QualType.h"

#include <map>
#include <string>
#include <vector>
#include <cstdint>

namespace trsc {
  
  enum class SymbolKind {
    SYMBOL_VARIABLE,
    SYMBOL_PARAMETER,
    SYMBOL_FUNCTION,
    SYMBOL_CONST,
  };

  enum class ScopeKind {
    SCOPE_GLOBAL,
    SCOPE_FORSTMT,
    SCOPE_BLOCKSTMT,
    SCOPE_WHILESTMT,
    SCOPE_FUNCTION,
  };

  const char* getSymbolKindName(SymbolKind Kind);
  const char* getScopeKindName(ScopeKind Kind);
  
  struct Symbol {
    mlir::memref::AllocaOp Alloca;
    QualType Ty;
    SymbolKind Kind;
    bool IsMutable;
    bool IsInitialized;

    Symbol(): Ty(), Kind(SymbolKind::SYMBOL_VARIABLE), 
    IsMutable(false), IsInitialized(true) {} 

    Symbol(SymbolKind Kind): Ty(), Kind(Kind), 
    IsMutable(false), IsInitialized(true) {}

    Symbol(SymbolKind Kind, bool IsInitialized): Ty(), 
     Kind(Kind), IsMutable(false), IsInitialized(IsInitialized) {}

    Symbol(QualType Ty,  SymbolKind Kind, bool IsMutable, 
     bool IsInitialized) : Ty(Ty), Kind(Kind), 
     IsMutable(IsMutable), IsInitialized(IsInitialized) {}

    void setAlloca(mlir::memref::AllocaOp Op) { this->Alloca = Op;}
  };

  class Scope {
    private:
      ScopeKind Kind;
      std::map<std::string, Symbol> Symbols;
      Scope* Parent;
      uint32_t Depth;
      std::vector<Scope*> Children;

    public:
      Scope(ScopeKind Kind, Scope* Parent, uint32_t Depth) : Kind(Kind), 
      Parent(Parent), Depth(Depth) {}

    ScopeKind getKind() const {return Kind;}
    Scope* getParent() const {return Parent;}
    uint32_t getDepth() const {return Depth;}
    const std::vector<Scope*>& getChildren() const {return Children;}
    const std::map<std::string, Symbol>& getSymbols() const {return Symbols;}

    bool canReturn() const {return Kind==ScopeKind::SCOPE_FUNCTION;}

    // Symbol Operation local to the current scope
    bool addSymbol(const std::string& Name, Symbol Sym);
    Symbol* lookupSymbolLocal(const std::string& Name);
  };

} // namespace trsc

#endif // TRSC_SEMA_SCOPE_H
