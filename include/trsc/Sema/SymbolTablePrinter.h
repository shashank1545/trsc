#ifndef TRSC_SEMA_SYMBOLTABLEPRINTER_H
#define TRSC_SEMA_SYMBOLTABLEPRINTER_H

#include "trsc/Sema/SymbolTable.h"
#include <cstdint>
#include <ostream>

namespace trsc {
  class SymbolTablePrinter {
    private:
      std::ostream &OS;
      const SymbolTable &ST;

      void printScope(const Scope* S, uint32_t IndentLevel);
      void indent(uint32_t Level);

    public:
      SymbolTablePrinter(std::ostream &OS, const SymbolTable &ST): 
        OS(OS), ST(ST) {}

      void print();
      void printTree();
  };
} // namespace trsc

#endif // TRSC_SEMA_SYMBOLTABLEPRINTER_H
