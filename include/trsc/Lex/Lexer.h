#ifndef TRSC_LEX_LEXER_H
#define TRSC_LEX_LEXER_H

#include "trsc/Lex/Token.h"
#include "trsc/Basic/Diagnostics.h"
#include "trsc/Basic/SourceManager.h"

namespace trsc {
  namespace Lex {

    class Lexer {
      public:
        Lexer(SourceManager &SM, DiagnosticsEngine &Diag);

        Token Lex();

      private:

        SourceManager &SM;
        DiagnosticsEngine &Diag;

        const char *BufferStart;
        const char *BufferEnd;
        const char *CurPtr;

        Token FormToken(TokenKind Kind, const char *TokenStart);

        void SkipWhiteSpace();

        void SkipLineComment();

        Token LexNumber(const char *Result);

        Token LexIdentifierOrKeyword(const char *Result);

        Token LexStringLiteral(const char *Result);

    };
  }
}

#endif
