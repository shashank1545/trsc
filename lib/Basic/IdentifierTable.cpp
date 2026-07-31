#include "trsc/Basic/IdentifierTable.h"

using namespace trsc;
using trsc::Lex::TokenKind;

IdentifierInfo *IdentifierTable::insert(std::string_view Spelling,
                                        TokenKind Kind) {
  // Build the value first: the key must view the stored Name, never the
  // caller's spelling, which is typically a view into the source buffer.
  std::unique_ptr<IdentifierInfo> Info(
      new IdentifierInfo(std::string(Spelling), Kind));
  std::string_view Key(Info->getName());
  return Table.emplace(Key, std::move(Info)).first->second.get();
}

IdentifierTable::IdentifierTable() {
  insert("fn", TokenKind::KW_FN);
  insert("let", TokenKind::KW_LET);
  insert("mut", TokenKind::KW_MUT);
  insert("const", TokenKind::KW_CONST);
  insert("as", TokenKind::KW_AS);
  insert("if", TokenKind::KW_IF);
  insert("else", TokenKind::KW_ELSE);
  insert("return", TokenKind::KW_RETURN);
  insert("true", TokenKind::KW_TRUE);
  insert("false", TokenKind::KW_FALSE);
  insert("while", TokenKind::KW_WHILE);
  insert("for", TokenKind::KW_FOR);
  insert("in", TokenKind::KW_IN);
}

IdentifierInfo *IdentifierTable::get(std::string_view Spelling) {
  auto It = Table.find(Spelling);
  if (It != Table.end())
    return It->second.get();
  return insert(Spelling, TokenKind::IDENTIFIER);
}

const IdentifierInfo *IdentifierTable::find(std::string_view Spelling) const {
  auto It = Table.find(Spelling);
  return It != Table.end() ? It->second.get() : nullptr;
}
