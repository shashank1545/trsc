#ifndef TRSC_BASIC_ARRAYREF_H
#define TRSC_BASIC_ARRAYREF_H

#include <cstddef>
#include <vector>

namespace trsc {

/// Non-owning view over a contiguous run of T. AST nodes hand these out over
/// their arena-allocated child arrays; the arena outlives every consumer, so
/// the view never dangles.
template <typename T> class ArrayRef {
  const T *Data = nullptr;
  size_t Length = 0;

public:
  ArrayRef() = default;
  ArrayRef(const T *Data, size_t Length) : Data(Data), Length(Length) {}
  ArrayRef(const std::vector<T> &Vec) : Data(Vec.data()), Length(Vec.size()) {}

  const T *begin() const { return Data; }
  const T *end() const { return Data + Length; }
  const T *data() const { return Data; }
  size_t size() const { return Length; }
  bool empty() const { return Length == 0; }

  const T &operator[](size_t Idx) const { return Data[Idx]; }
  const T &front() const { return Data[0]; }
  const T &back() const { return Data[Length - 1]; }
};

} // namespace trsc

#endif // TRSC_BASIC_ARRAYREF_H
