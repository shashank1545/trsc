#include "trsc/Sema/SymbolTablePrinter.h"
#include "trsc/Basic/IdentifierTable.h"
#include "trsc/Sema/Scope.h"
#include <algorithm>
#include <iomanip>

namespace trsc {

namespace {
Scope::EntryList sortedByName(const Scope *S) {
  Scope::EntryList Sorted = S->getSymbols();
  std::sort(Sorted.begin(), Sorted.end(), [](const auto &A, const auto &B) {
    return A.first->getName() < B.first->getName();
  });
  return Sorted;
}
} // namespace

void SymbolTablePrinter::indent(unsigned Level) {
  for (unsigned i = 0; i < Level; ++i) {
    OS << "  ";
  }
}

void SymbolTablePrinter::printScope(const Scope *S, unsigned IndentLevel) {
  indent(IndentLevel);
  OS << "┌─ " << getScopeKindName(S->getKind())
     << " Scope (Depth: " << S->getDepth() << ")\n";

  // Print symbols
  const auto Symbols = sortedByName(S);
  if (Symbols.empty()) {
    indent(IndentLevel);
    OS << "│  (no symbols)\n";
  } else {
    for (const auto &[Name, Sym] : Symbols) {
      indent(IndentLevel);
      OS << "│  " << std::setw(20) << std::left << Name->getName();
      OS << " : " << std::setw(12) << std::left
         << (Sym->Ty.isNull() ? "<unresolved>" : Sym->Ty.getAsString());
      OS << " [" << getSymbolKindName(Sym->Kind) << "]";
      if (Sym->IsMutable)
        OS << " mut";
      if (Sym->IsInitialized)
        OS << " init";

      OS << "\n";
    }
  }

  indent(IndentLevel);
  OS << "└─\n";
}

void SymbolTablePrinter::print() {
  OS << "\n========== SYMBOL TABLE DUMP ==========\n";

  const auto &AllScopes = ST.getAllScopes();
  OS << "Total scopes: " << AllScopes.size() << "\n\n";

  for (size_t i = 0; i < AllScopes.size(); ++i) {
    const Scope *S = AllScopes[i].get();
    OS << "Scope #" << i << " - " << getScopeKindName(S->getKind())
       << " (Depth: " << S->getDepth() << ")\n";

    const auto Symbols = sortedByName(S);
    if (Symbols.empty()) {
      OS << "  (no symbols)\n";
    } else {
      for (const auto &[Name, Sym] : Symbols) {
        OS << "  " << std::setw(20) << std::left << Name->getName() << " : "
           << (Sym->Ty.isNull() ? "<unresolved>" : Sym->Ty.getAsString())
           << " [" << getSymbolKindName(Sym->Kind) << "]";
        if (Sym->IsMutable)
          OS << " mut";
        OS << "\n";
      }
    }
    OS << "\n";
  }

  OS << "=======================================\n\n";
}

void SymbolTablePrinter::printTree() {
  OS << "\n========== SYMBOL TABLE TREE ==========\n\n";
  // Scopes are stored in creation (preorder) order; indenting each one by its
  // depth reproduces the nesting without needing child links on Scope.
  for (const auto &S : ST.getAllScopes()) {
    printScope(S.get(), S->getDepth());
  }
  OS << "=======================================\n\n";
}

} // namespace trsc
