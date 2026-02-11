#ifndef TRSC_BASIC_DIAGONISTICS_H
#define TRSC_BASIC_DIAGONISTICS_H

#include "SourceLocation.h"
#include <string>

namespace trsc {
  enum class DiagKind {
    Note,
    Warning,
    Error
  };

  class DiagnosticsEngine {
    public:
      DiagnosticsEngine() = default;

      void Report(DiagKind Kind, const std::string &Message,
                SourceLocation Loc = {});

      unsigned getNumErrors() const { return NumErrors; }

      unsigned getNumWarnings() const { return NumWarnings; }

    private:
      unsigned NumErrors = 0;
      unsigned NumWarnings = 0;
  };
}

#endif
