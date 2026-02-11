#ifndef TRSC_BASIC_SOURCELOCATION_H
#define TRSC_BASIC_SOURCELOCATION_H

namespace trsc {

  struct SourceLocation {
    const char* FilePath;
    unsigned Line;
    unsigned Column;

    SourceLocation() : FilePath(nullptr), Line(0), Column(0) {}

    SourceLocation(const char* File, unsigned L, unsigned C) :
      FilePath(File), Line(L), Column(C) {}

    bool isValid() const { return FilePath != nullptr; }
  };

  struct SourceRange {
    SourceLocation Start;
    SourceLocation End;

    SourceRange() = default;
    SourceRange(SourceLocation Start, SourceLocation End) : Start(Start), End(End) {}

    SourceLocation getStart() const { return Start; }
    SourceLocation getEnd() const { return End; }
  };
}

#endif
