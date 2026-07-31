#ifndef TRSC_BASIC_IDENTIFIERTABLE_H
#define TRSC_BASIC_IDENTIFIERTABLE_H

#include "trsc/Lex/Token.h"

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

namespace trsc {

// One IdentifierInfo per unique spelling in the translation unit. Addresses are
// stable for the lifetime of the owning IdentifierTable, so everything
// downstream of the lexer compares names by pointer rather than by content.
class IdentifierInfo {
  friend class IdentifierTable;

  std::string Name;
  Lex::TokenKind TokKind;

  IdentifierInfo(std::string Name, Lex::TokenKind TokKind)
      : Name(std::move(Name)), TokKind(TokKind) {}

public:
  IdentifierInfo(const IdentifierInfo &) = delete;
  IdentifierInfo &operator=(const IdentifierInfo &) = delete;

  const std::string &getName() const { return Name; }

  // The keyword this spelling denotes, or TokenKind::IDENTIFIER. Lets the lexer
  // classify keywords with a table probe instead of a chain of comparisons.
  Lex::TokenKind getTokenKind() const { return TokKind; }
  bool isKeyword() const { return TokKind != Lex::TokenKind::IDENTIFIER; }
};

class IdentifierTable {
  // Keys are views into each value's own Name, so a spelling is stored exactly
  // once. unique_ptr keeps IdentifierInfo addresses stable across rehashing.
  std::unordered_map<std::string_view, std::unique_ptr<IdentifierInfo>> Table;

  IdentifierInfo *insert(std::string_view Spelling, Lex::TokenKind Kind);

public:
  // Pre-populates the language keywords.
  IdentifierTable();

  IdentifierTable(const IdentifierTable &) = delete;
  IdentifierTable &operator=(const IdentifierTable &) = delete;

  // Returns the unique IdentifierInfo for Spelling, creating it if absent.
  IdentifierInfo *get(std::string_view Spelling);

  // Like get(), but returns nullptr instead of interning an unseen spelling.
  // For queries that must not grow the table.
  const IdentifierInfo *find(std::string_view Spelling) const;

  std::size_t size() const { return Table.size(); }
};

} // namespace trsc

#endif // TRSC_BASIC_IDENTIFIERTABLE_H
