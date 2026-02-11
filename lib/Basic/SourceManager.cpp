#include "trsc/Basic/SourceManager.h"
#include "trsc/Basic/Diagnostics.h"
#include <fstream>
#include <algorithm>
#include <iterator>

namespace trsc {
SourceManager::SourceManager(DiagnosticsEngine &Diag) : Diag(Diag) {}
bool SourceManager::loadFile(const std::string &FilePath) {
  MainFilePath = FilePath;

  std::ifstream file(MainFilePath, std::ios::binary | std::ios::ate);
  if (!file.is_open()) {
    Diag.Report(DiagKind::Error, "File Not Found");
    return false;
  }
  std::streamsize size = file.tellg();
  file.seekg(0, std::ios::beg);
  Buffer.resize(size);
  if (!file.read(Buffer.data(), size)) {
    Diag.Report(DiagKind::Error, "Could not read file");
    return false;
  }
  buildLineStartCache();
  return true;
}

  const char* SourceManager::getBufferStart() const {
    return Buffer.data();
  }

  const char* SourceManager::getBufferEnd() const {
    return Buffer.data() + Buffer.size();
  }

  void SourceManager::buildLineStartCache() {

    LineStartCache.clear();

    LineStartCache.push_back(getBufferStart());

    const char* Ptr = getBufferStart();
    const char* End = getBufferEnd();

    while (Ptr < End) {
      if (*Ptr == '\n') {
        LineStartCache.push_back(Ptr+1);
      }
      Ptr++;
    }
  }

  SourceLocation SourceManager::getLocation(const char* Ptr) const {
    auto it = std::lower_bound(LineStartCache.begin(), LineStartCache.end(), Ptr);

    if (it != LineStartCache.begin() && *it > Ptr) {
      --it;
    }

    unsigned Line = std::distance(LineStartCache.begin(), it) + 1;
    unsigned Column = (Ptr - *it) + 1;

    return SourceLocation(MainFilePath.c_str(), Line, Column);
  }
}
