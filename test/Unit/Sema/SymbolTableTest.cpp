#include "trsc/Sema/SymbolTable.h"
#include "trsc/Basic/IdentifierTable.h"
#include "trsc/Sema/Scope.h"
#include <gtest/gtest.h>
#include <string>
#include <vector>

using namespace trsc;

namespace {

class SymbolTableTest : public ::testing::Test {
protected:
  IdentifierTable Idents;
  SymbolTable Table{Idents};

  const IdentifierInfo *id(const char *Name) { return Idents.get(Name); }
};

TEST_F(SymbolTableTest, BasicScoping) {
  Table.enterScope(ScopeKind::SCOPE_FUNCTION);
  EXPECT_NE(Table.addSymbol(id("x"), Symbol()), nullptr);
  EXPECT_NE(Table.lookupSymbol(id("x")), nullptr);

  Table.exitScope();
  EXPECT_EQ(Table.lookupSymbol(id("x")), nullptr);
}

TEST_F(SymbolTableTest, LookupWalksToEnclosingScope) {
  ASSERT_NE(Table.addSymbol(id("g"), Symbol()), nullptr);

  Table.enterScope(ScopeKind::SCOPE_FUNCTION);
  Table.enterScope(ScopeKind::SCOPE_BLOCKSTMT);
  EXPECT_NE(Table.lookupSymbol(id("g")), nullptr);
  EXPECT_EQ(Table.lookupSymbol(id("absent")), nullptr);
}

TEST_F(SymbolTableTest, RedefinitionInSameScopeRejected) {
  Table.enterScope(ScopeKind::SCOPE_FUNCTION);
  EXPECT_NE(Table.addSymbol(id("x"), Symbol()), nullptr);
  EXPECT_EQ(Table.addSymbol(id("x"), Symbol()), nullptr);
}

TEST_F(SymbolTableTest, InnerScopeShadowsOuter) {
  Table.enterScope(ScopeKind::SCOPE_FUNCTION);
  Symbol *Outer = Table.addSymbol(id("x"), Symbol());
  ASSERT_NE(Outer, nullptr);

  Table.enterScope(ScopeKind::SCOPE_BLOCKSTMT);
  Symbol *Inner = Table.addSymbol(id("x"), Symbol());
  ASSERT_NE(Inner, nullptr);
  EXPECT_NE(Inner, Outer);
  EXPECT_EQ(Table.lookupSymbol(id("x")), Inner);

  Table.exitScope();
  EXPECT_EQ(Table.lookupSymbol(id("x")), Outer);
}

// NameResolver caches these pointers and MLIRGen mutates them several passes
// later, so an insert must never relocate an existing Symbol.
TEST_F(SymbolTableTest, SymbolPointersSurviveLaterInserts) {
  Table.enterScope(ScopeKind::SCOPE_FUNCTION);
  std::vector<Symbol *> Handles;
  std::vector<std::string> Names;
  Names.reserve(500);
  for (int I = 0; I < 500; ++I) {
    Names.push_back("v" + std::to_string(I));
  }
  for (int I = 0; I < 500; ++I) {
    Symbol *S = Table.addSymbol(id(Names[I].c_str()), Symbol());
    ASSERT_NE(S, nullptr);
    S->Op = reinterpret_cast<void *>(static_cast<intptr_t>(I + 1)); // NOLINT
    Handles.push_back(S);
  }
  for (int I = 0; I < 500; ++I) {
    EXPECT_EQ(Handles[I]->Op,
              reinterpret_cast<void *>(static_cast<intptr_t>(I + 1))); // NOLINT
    EXPECT_EQ(Table.lookupSymbol(id(Names[I].c_str())), Handles[I]);
  }
}

// Exercises the same-spelling path through both key types, including the
// promotion to the hash index once a scope outgrows the linear scan.
TEST_F(SymbolTableTest, SpellingAndPointerKeysAgree) {
  Table.enterScope(ScopeKind::SCOPE_FUNCTION);
  std::vector<std::string> Names;
  Names.reserve(64);
  for (int I = 0; I < 64; ++I) {
    Names.push_back("sym" + std::to_string(I));
  }
  for (const auto &N : Names) {
    ASSERT_NE(Table.addSymbol(N, Symbol()), nullptr);
  }
  for (const auto &N : Names) {
    Symbol *ByString = Table.lookupSymbol(N);
    ASSERT_NE(ByString, nullptr);
    EXPECT_EQ(ByString, Table.lookupSymbol(id(N.c_str())));
    EXPECT_EQ(ByString->Name, id(N.c_str()));
  }
  EXPECT_EQ(Table.lookupSymbol(std::string("never_declared")), nullptr);
}

} // namespace
