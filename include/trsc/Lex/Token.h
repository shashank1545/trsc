#ifndef TRSC_LEX_TOKEN_H
#define TRSC_LEX_TOKEN_H

#include "trsc/Basic/SourceLocation.h"
#include <string_view>

namespace trsc {

class IdentifierInfo;

namespace Lex {

enum class TokenKind {
  UNKNOWN,
  ENDOFFILE,

  // KEYWORDS
  KW_FN,
  KW_LET,
  KW_MUT,
  KW_CONST,
  KW_AS,
  KW_IF,
  KW_ELSE,
  KW_RETURN,
  KW_TRUE,
  KW_FALSE,
  KW_WHILE,
  KW_FOR,
  KW_IN,

  // OPERATORS
  OP_PLUS,
  OP_MINUS,
  OP_STAR,
  OP_SLASH,
  OP_PERCENT,
  OP_EQUAL,
  OP_EQUALEQUAL,
  OP_BANG,
  OP_BANGEQUAL,
  OP_LESS,
  OP_LESSEQUAL,
  OP_GREATER,
  OP_GREATEREQUAL,
  OP_COLONCOLON,
  OP_PLUSEQUAL,
  OP_MINUSEQUAL,
  OP_STARSTAR,
  OP_LESSLESS,
  OP_GREATERGREATER,
  OP_DOTDOT,
  OP_DOTDOTEQUAL,
  OP_AMP,
  OP_AMPAMP,
  OP_PIPE,
  OP_PIPEPIPE,

  // DELIMITERS
  DE_LPAREN,
  DE_RPAREN,
  DE_LBRACE,
  DE_RBRACE,
  DE_LBRACKET,
  DE_RBRACKET,
  DE_COMMA,
  DE_SEMICOLON,
  DE_COLON,
  DE_DOT,
  DE_RETURNTYPE,

  // LITERALS
  LT_INTEGER,
  LT_FLOAT,
  LT_STRING,

  // IDENTIFIER
  IDENTIFIER,
};

const char *getTokenName(TokenKind K);
const char *getTokenString(TokenKind K);

class Token {

  TokenKind Kind;
  SourceLocation Loc;
  // Length is Text.size(); the view carries both pointer and extent.
  std::string_view Text;
  // Set for identifiers and keywords, null for everything else.
  IdentifierInfo *Id;

public:
  Token()
      : Kind(TokenKind::UNKNOWN), Loc({nullptr, 0, 0}), Text(""), Id(nullptr) {}

  Token(TokenKind K, SourceLocation L, std::string_view T,
        IdentifierInfo *Id = nullptr)
      : Kind(K), Loc(L), Text(T), Id(Id) {}

  TokenKind getKind() const { return Kind; }
  SourceLocation getLocation() const { return Loc; }
  std::string_view getText() const { return Text; }
  IdentifierInfo *getIdentifierInfo() const { return Id; }

  bool is(TokenKind K) const { return Kind == K; }
};

} // namespace Lex
} // namespace trsc

#endif // TRSC_LEX_TOKEN_H
