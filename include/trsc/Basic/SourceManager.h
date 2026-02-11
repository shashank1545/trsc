#ifndef TRSC_BASIC_SOURCEMANAGER_H
#define TRSC_BASIC_SOURCEMANAGER_H

#include "SourceLocation.h"
#include <string>
#include <vector>

namespace trsc {

class DiagnosticsEngine;

class SourceManager {
  public:
    SourceManager(DiagnosticsEngine &Diag);

    bool loadFile(const std::string &FilePath);

    const char *getBufferStart() const;

    const char *getBufferEnd() const;

    SourceLocation getLocation(const char *Ptr) const;

  private:
    DiagnosticsEngine &Diag;
    std::string MainFilePath;
    std::vector<char> Buffer;

    std::vector<const char *> LineStartCache;
    void buildLineStartCache();
};
}

#endif
