#include "trsc/Basic/Diagnostics.h"
#include "trsc/Basic/SourceLocation.h"
#include <iostream>

namespace trsc {
  void DiagnosticsEngine::Report(DiagKind Kind, const std::string &Message,
                                 SourceLocation Loc) {
    if (Kind == DiagKind::Error) {
      NumErrors++;
    }
    else if (Kind == DiagKind::Warning) {
      NumWarnings++;
    }
    
    std::cerr << Loc.FilePath << ":" << Loc.Line << ":" << Loc.Column << ": ";

    switch (Kind) {
      case DiagKind::Note:
        std::cerr << "note: ";
        break;
      case DiagKind::Warning:
        std::cerr << "Warning: ";
        break;
      case DiagKind::Error:
        std::cerr << "Error: ";
        break;
    }
    std::cerr << Message << std::endl;
    } 
}
