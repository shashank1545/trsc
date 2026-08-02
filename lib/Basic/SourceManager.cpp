#include "trsc/Basic/SourceManager.h"
#include "trsc/Basic/Diagnostics.h"
#include <fstream>

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

void SourceManager::loadBuffer(const std::string &Source,
                               const std::string &BufferName) {
  MainFilePath = BufferName;
  Buffer.assign(Source.begin(), Source.end());
  buildLineStartCache();
}

const char *SourceManager::getBufferStart() const { return Buffer.data(); }

const char *SourceManager::getBufferEnd() const {
  return Buffer.data() + Buffer.size();
}

void SourceManager::buildLineStartCache() {

  LineStartCache.clear();
  LastLineIdx = 0;

  LineStartCache.push_back(getBufferStart());

  const char *Ptr = getBufferStart();
  const char *End = getBufferEnd();

  while (Ptr < End) {
    if (*Ptr == '\n') {
      LineStartCache.push_back(Ptr + 1);
    }
    Ptr++;
  }
}

SourceLocation SourceManager::getLocation(const char *Ptr) const {
  // Fast path: same line as the previous query, or the next one.
  auto onLine = [this, Ptr](size_t Idx) {
    return Ptr >= LineStartCache[Idx] &&
           (Idx + 1 == LineStartCache.size() || Ptr < LineStartCache[Idx + 1]);
  };
  if (LastLineIdx < LineStartCache.size()) {
    if (onLine(LastLineIdx)) {
      return SourceLocation(MainFilePath.c_str(), LastLineIdx + 1,
                            (Ptr - LineStartCache[LastLineIdx]) + 1);
    }
    if (LastLineIdx + 1 < LineStartCache.size() && onLine(LastLineIdx + 1)) {
      ++LastLineIdx;
      return SourceLocation(MainFilePath.c_str(), LastLineIdx + 1,
                            (Ptr - LineStartCache[LastLineIdx]) + 1);
    }
  }

  auto it = std::lower_bound(LineStartCache.begin(), LineStartCache.end(), Ptr);

  if (it != LineStartCache.begin() &&
      (it == LineStartCache.end() || *it > Ptr)) {
    --it;
  }

  LastLineIdx = std::distance(LineStartCache.begin(), it);
  unsigned Line = LastLineIdx + 1;
  unsigned Column = (Ptr - *it) + 1;

  return SourceLocation(MainFilePath.c_str(), Line, Column);
}
} // namespace trsc
