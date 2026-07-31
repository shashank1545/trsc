#ifndef TRSC_SEMA_SCOPE_H
#define TRSC_SEMA_SCOPE_H

#include "trsc/AST/QualType.h"

#include <cstddef>
#include <memory>
#include <unordered_map>
#include <utility>

namespace trsc {

class IdentifierInfo;

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

const char *getSymbolKindName(SymbolKind Kind);
const char *getScopeKindName(ScopeKind Kind);

class Scope;

struct Symbol {
  void *Op = nullptr;
  Scope *Scp = nullptr;
  const IdentifierInfo *Name = nullptr;
  QualType Ty;
  SymbolKind Kind;
  bool IsMutable;
  bool IsInitialized;

  Symbol()
      : Ty(), Kind(SymbolKind::SYMBOL_VARIABLE), IsMutable(false),
        IsInitialized(true) {}

  Symbol(SymbolKind Kind)
      : Ty(), Kind(Kind), IsMutable(false), IsInitialized(true) {}

  Symbol(SymbolKind Kind, bool IsInitialized)
      : Ty(), Kind(Kind), IsMutable(false), IsInitialized(IsInitialized) {}

  Symbol(QualType Ty, SymbolKind Kind, bool IsMutable, bool IsInitialized)
      : Ty(Ty), Kind(Kind), IsMutable(IsMutable), IsInitialized(IsInitialized) {
  }

  void setOp(void *OpPtr) { this->Op = OpPtr; }
  void setScope(Scope *S) { this->Scp = S; }
  Scope *getScope() { return this->Scp; }

  template <typename T> T getOpAs() const { return reinterpret_cast<T>(Op); }
};

class Scope {
public:
  using EntryList = std::vector<std::pair<const IdentifierInfo *, Symbol *>>;

private:
  ScopeKind Kind;
  EntryList Symbols;
  std::unique_ptr<std::unordered_map<const IdentifierInfo *, Symbol *>> Index;
  Scope *Parent;
  uint32_t Depth;
  std::vector<Scope *> Children;

  static constexpr std::size_t IndexThreshold = 32;
  void buildIndex();

public:
  Scope(ScopeKind Kind, Scope *Parent, uint32_t Depth)
      : Kind(Kind), Parent(Parent), Depth(Depth) {}

  ScopeKind getKind() const { return Kind; }
  Scope *getParent() const { return Parent; }
  uint32_t getDepth() const { return Depth; }
  const std::vector<Scope *> &getChildren() const { return Children; }
  const EntryList &getSymbols() const { return Symbols; }

  bool canReturn() const { return Kind == ScopeKind::SCOPE_FUNCTION; }

  Symbol *addSymbol(const IdentifierInfo *Name, Symbol *Sym);
  Symbol *lookupSymbolLocal(const IdentifierInfo *Name);
};

} // namespace trsc

#endif // TRSC_SEMA_SCOPE_H
