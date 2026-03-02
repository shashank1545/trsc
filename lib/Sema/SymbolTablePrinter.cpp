#include "trsc/Sema/SymbolTablePrinter.h"
#include "trsc/Sema/Scope.h"
#include <iomanip>

namespace trsc {

  void SymbolTablePrinter::indent(unsigned Level) {
    for (unsigned i = 0; i < Level; ++i) {
      OS << "  ";
    }
  }

  void SymbolTablePrinter::printScope(const Scope* S, unsigned IndentLevel) {
    indent(IndentLevel);
    OS << "┌─ " << getScopeKindName(S->getKind()) 
      << " Scope (Depth: " << S->getDepth() << ")\n";

    // Print symbols
    const auto& Symbols = S->getSymbols();
    if (Symbols.empty()) {
      indent(IndentLevel);
      OS << "│  (no symbols)\n";
    } else {
      for (const auto& [Name, Sym] : Symbols) {
        indent(IndentLevel);
        OS << "│  " << std::setw(20) << std::left << Name;
        OS << " : " << std::setw(12) << std::left 
          << (Sym.Ty.isNull() ? "<unresolved>" : Sym.Ty.getAsString());
        OS << " [" << getSymbolKindName(Sym.Kind) << "]";
        if (Sym.IsMutable) OS << " mut";
        if (Sym.IsInitialized) OS << " init";

        // if (Sym.Node) {
        //   auto Loc = Sym.Node->getSourceRange().getStart();
        //   if (Loc.isValid()) {
        //     OS << "  @ " << Loc.Line << ":" << Loc.Column;
        //   }
        // }
        OS << "\n";
      }
    }

    // Print children recursively
    const auto& Children = S->getChildren();
    for (size_t i = 0; i < Children.size(); ++i) {
      indent(IndentLevel);
      OS << "│\n";
      printScope(Children[i], IndentLevel + 1);
    }

    indent(IndentLevel);
    OS << "└─\n";
  }

  void SymbolTablePrinter::print() {
    OS << "\n========== SYMBOL TABLE DUMP ==========\n";

    const auto& AllScopes = ST.getAllScopes();
    OS << "Total scopes: " << AllScopes.size() << "\n\n";

    for (size_t i = 0; i < AllScopes.size(); ++i) {
      const Scope* S = AllScopes[i].get();
      OS << "Scope #" << i << " - " << getScopeKindName(S->getKind())
        << " (Depth: " << S->getDepth() << ")\n";

      const auto& Symbols = S->getSymbols();
      if (Symbols.empty()) {
        OS << "  (no symbols)\n";
      } else {
        for (const auto& [Name, Sym] : Symbols) {
          OS << "  " << std::setw(20) << std::left << Name
            << " : " << (Sym.Ty.isNull() ? "<unresolved>" : Sym.Ty.getAsString())
            << " [" << getSymbolKindName(Sym.Kind) << "]";
          if (Sym.IsMutable) OS << " mut";
          OS << "\n";
        }
      }
      OS << "\n";
    }

    OS << "=======================================\n\n";
  }

  void SymbolTablePrinter::printTree() {
    OS << "\n========== SYMBOL TABLE TREE ==========\n\n";
    printScope(ST.getGlobalScope(), 0);
    OS << "=======================================\n\n";
  }

} // namespace trsc

