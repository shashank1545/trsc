#include "trsc/Sema/Scope.h"

namespace trsc {

const char *getSymbolKindName(SymbolKind Kind) {
  switch (Kind) {
  case trsc::SymbolKind::SYMBOL_VARIABLE:
    return "Variable";
  case trsc::SymbolKind::SYMBOL_PARAMETER:
    return "Parameter";
  case trsc::SymbolKind::SYMBOL_FUNCTION:
    return "Function";
  case trsc::SymbolKind::SYMBOL_CONST:
    return "Const";
  }
  return "Unknown";
}

const char *getScopeKindName(ScopeKind Kind) {
  switch (Kind) {
  case trsc::ScopeKind::SCOPE_GLOBAL:
    return "Global";
  case trsc::ScopeKind::SCOPE_FORSTMT:
    return "Forstmt";
  case trsc::ScopeKind::SCOPE_WHILESTMT:
    return "Whilestmt";
  case trsc::ScopeKind::SCOPE_BLOCKSTMT:
    return "Blockstmt";
  case trsc::ScopeKind::SCOPE_FUNCTION:
    return "Function";
  }
  return "Unknown";
}

void Scope::buildIndex() {
  Index =
      std::make_unique<std::unordered_map<const IdentifierInfo *, Symbol *>>();
  Index->reserve(Symbols.size() * 2);
  for (const auto &Entry : Symbols)
    Index->emplace(Entry.first, Entry.second);
}

Symbol *Scope::addSymbolUnchecked(const IdentifierInfo *Name, Symbol *Sym) {
  Symbols.emplace_back(Name, Sym);
  if (Index)
    Index->emplace(Name, Sym);
  else if (Symbols.size() > IndexThreshold)
    buildIndex();
  return Sym;
}

Symbol *Scope::lookupSymbolLocal(const IdentifierInfo *Name) {
  if (Index) {
    auto It = Index->find(Name);
    return It != Index->end() ? It->second : nullptr;
  }
  for (const auto &Entry : Symbols) {
    if (Entry.first == Name)
      return Entry.second;
  }
  return nullptr;
}

} // namespace trsc
