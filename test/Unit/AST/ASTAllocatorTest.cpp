#include "trsc/AST/ASTAllocator.h"

#include <cstdint>
#include <gtest/gtest.h>

namespace {

using trsc::ASTAllocator;

bool isAligned(void *Ptr, size_t Align) {
  return reinterpret_cast<uintptr_t>(Ptr) % Align == 0;
}

TEST(ASTAllocatorTest, ReturnsUsableMemory) {
  ASTAllocator A;
  auto *P = static_cast<int *>(A.Allocate(sizeof(int), alignof(int)));
  ASSERT_NE(P, nullptr);
  *P = 42;
  EXPECT_EQ(*P, 42);
}

TEST(ASTAllocatorTest, RespectsAlignment) {
  ASTAllocator A;
  // Mix odd sizes with strict alignments to force padding.
  A.Allocate(1, 1);
  void *P8 = A.Allocate(8, 8);
  EXPECT_TRUE(isAligned(P8, 8));
  A.Allocate(3, 1);
  void *P16 = A.Allocate(16, 16);
  EXPECT_TRUE(isAligned(P16, 16));
  A.Allocate(5, 1);
  void *PD = A.Allocate(sizeof(double), alignof(double));
  EXPECT_TRUE(isAligned(PD, alignof(double)));
}

TEST(ASTAllocatorTest, AllocationsDoNotOverlap) {
  ASTAllocator A;
  auto *P1 = static_cast<uint64_t *>(A.Allocate(8, 8));
  auto *P2 = static_cast<uint64_t *>(A.Allocate(8, 8));
  *P1 = 0x1111111111111111ULL;
  *P2 = 0x2222222222222222ULL;
  EXPECT_EQ(*P1, 0x1111111111111111ULL);
  EXPECT_EQ(*P2, 0x2222222222222222ULL);
}

TEST(ASTAllocatorTest, GrowsAcrossSlabs) {
  ASTAllocator A;
  EXPECT_EQ(A.getNumSlabs(), 0u);
  // 4 KB first slab: 100 * 64 bytes overflows it and forces growth.
  for (int I = 0; I < 100; ++I) {
    void *P = A.Allocate(64, 8);
    ASSERT_NE(P, nullptr);
  }
  EXPECT_GE(A.getNumSlabs(), 2u);
  EXPECT_GE(A.getTotalMemory(), 100u * 64u);
}

TEST(ASTAllocatorTest, OversizedRequestGetsOwnSlab) {
  ASTAllocator A;
  void *P = A.Allocate(1024 * 1024, 8); // larger than the 256 KB cap
  ASSERT_NE(P, nullptr);
  // Must still be usable end to end.
  auto *Bytes = static_cast<char *>(P);
  Bytes[0] = 1;
  Bytes[1024 * 1024 - 1] = 2;
  EXPECT_EQ(Bytes[0], 1);
  EXPECT_EQ(Bytes[1024 * 1024 - 1], 2);
}

} // namespace
