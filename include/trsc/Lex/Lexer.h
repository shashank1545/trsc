#ifndef TRSC_LEX_LEXER_H
#define TRSC_LEX_LEXER_H

#include "trsc/Basic/Diagnostics.h"
#include "trsc/Basic/SourceManager.h"
#include "trsc/Lex/Token.h"

namespace trsc {

class IdentifierTable;

namespace Lex {

class Lexer {
public:
  // Idents must outlive the Lexer: the IdentifierInfo pointers it hands out are
  // retained by the AST and the symbol table.
  Lexer(SourceManager &SM, DiagnosticsEngine &Diag, IdentifierTable &Idents);

  Token Lex();

private:
  SourceManager &SM;
  DiagnosticsEngine &Diag;
  IdentifierTable &Idents;

  const char *BufferStart;
  const char *BufferEnd;
  const char *CurPtr;

  Token FormToken(TokenKind Kind, const char *TokenStart,
                  IdentifierInfo *Id = nullptr);

  void SkipWhiteSpace();

  void SkipLineComment();

  Token LexNumber(const char *Result);

  Token LexIdentifierOrKeyword(const char *Result);

  Token LexStringLiteral(const char *Result);
};
} // namespace Lex
} // namespace trsc

#endif
