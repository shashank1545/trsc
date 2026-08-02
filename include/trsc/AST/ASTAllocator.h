#ifndef TRSC_AST_ASTALLOCATOR_H
#define TRSC_AST_ASTALLOCATOR_H

#include <cstddef>
#include <cstdlib>
#include <memory>
#include <vector>

namespace trsc {

/// Bump allocator backing the AST. Nodes are allocated in slabs and freed all
/// at once when the owning ASTContext dies; individual nodes are never
/// destroyed, so everything placed here must be trivially destructible.
class ASTAllocator {
  std::vector<void *> Slabs;
  char *Cur = nullptr;
  char *End = nullptr;
  size_t TotalBytes = 0;
  size_t NextSlabSize = 4096;
  static constexpr size_t MaxSlabSize = size_t{256} * 1024;

  void newSlab(size_t MinBytes) {
    size_t Size = NextSlabSize;
    if (Size < MinBytes) {
      Size = MinBytes;
    }
    void *Slab = std::malloc(Size);
    Slabs.push_back(Slab);
    Cur = static_cast<char *>(Slab);
    End = Cur + Size;
    TotalBytes += Size;
    if (NextSlabSize < MaxSlabSize) {
      NextSlabSize *= 2;
    }
  }

public:
  ASTAllocator() = default;
  ASTAllocator(const ASTAllocator &) = delete;
  ASTAllocator &operator=(const ASTAllocator &) = delete;

  ~ASTAllocator() {
    for (void *Slab : Slabs) {
      std::free(Slab);
    }
  }

  void *Allocate(size_t Bytes, size_t Align) {
    // std::align adjusts Cur forward to the requested alignment (avoids
    // integer round-up on pointers, which trips performance-no-int-to-ptr).
    void *Ptr = Cur;
    size_t Space = static_cast<size_t>(End - Cur);
    if (Cur == nullptr || !std::align(Align, Bytes, Ptr, Space)) {
      newSlab(Bytes + Align);
      Ptr = Cur;
      Space = static_cast<size_t>(End - Cur);
      std::align(Align, Bytes, Ptr, Space);
    }
    Cur = static_cast<char *>(Ptr) + Bytes;
    return Ptr;
  }

  size_t getNumSlabs() const { return Slabs.size(); }
  size_t getTotalMemory() const { return TotalBytes; }
};

} // namespace trsc

#endif // TRSC_AST_ASTALLOCATOR_H
