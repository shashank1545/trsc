#include "trsc/Lex/Lexer.h"
#include <cctype>
#include <string_view>

namespace trsc {
  namespace Lex {

    Lexer::Lexer(SourceManager &SM, DiagnosticsEngine &Diag):
      SM(SM), Diag(Diag) {
        BufferStart = SM.getBufferStart();
        BufferEnd = SM.getBufferEnd();
        CurPtr = BufferStart;
      }

    Token Lexer::Lex() {

      SkipWhiteSpace();

      const char* TokenStart = CurPtr;

      if (CurPtr >= BufferEnd) {
        return FormToken(TokenKind::ENDOFFILE, TokenStart);
      }

      switch (*CurPtr) {
        case 'a': case 'b': case 'c': case 'd': case 'e': case'f': case 'g':
        case 'h': case 'i': case 'j': case 'k': case 'l': case'm': case 'n':
        case 'o': case 'p': case 'q': case 'r': case 's': case't': case 'u':
        case 'v': case 'w': case 'x': case 'y': case 'z': 
        case 'A': case 'B': case 'C': case 'D': case 'E': case'F': case 'G':
        case 'H': case 'I': case 'J': case 'K': case 'L': case'M': case 'N':
        case 'O': case 'P': case 'Q': case 'R': case 'S': case'T': case 'U':
        case 'V': case 'W': case 'X': case 'Y': case 'Z':
        case '_':
          return LexIdentifierOrKeyword(TokenStart);

        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
          return LexNumber(TokenStart);

        case '"':
          return LexStringLiteral(TokenStart);

        case '(': CurPtr++; return FormToken(TokenKind::DE_LPAREN, TokenStart);
        case ')': CurPtr++; return FormToken(TokenKind::DE_RPAREN, TokenStart);
        case '{': CurPtr++; return FormToken(TokenKind::DE_LBRACE, TokenStart);
        case '}': CurPtr++; return FormToken(TokenKind::DE_RBRACE, TokenStart);
        case '[': CurPtr++; return FormToken(TokenKind::DE_LBRACKET, TokenStart);
        case ']': CurPtr++; return FormToken(TokenKind::DE_RBRACKET, TokenStart);
        case ';': CurPtr++; return FormToken(TokenKind::DE_SEMICOLON, TokenStart);
        case ',': CurPtr++; return FormToken(TokenKind::DE_COMMA, TokenStart);
        case ':': 
            if (*(CurPtr + 1) == ':') {
              CurPtr += 2;
              return FormToken(TokenKind::OP_COLONCOLON, TokenStart);
            }
            CurPtr++;
            return FormToken(TokenKind::DE_COLON, TokenStart);
        case '-': 
            if(*(CurPtr + 1) == '>') {
              CurPtr += 2;
              return FormToken(TokenKind::DE_RETURNTYPE, TokenStart);
            }
            else if(*(CurPtr + 1) == '=') {
              CurPtr += 2;
              return FormToken(TokenKind::OP_MINUSEQUAL, TokenStart);
            }
            CurPtr++;
            return FormToken(TokenKind::OP_MINUS, TokenStart);
        case '+': 
            if(*(CurPtr + 1) == '=') {
              CurPtr += 2;
              return FormToken(TokenKind::OP_PLUSEQUAL, TokenStart);
            }
            CurPtr++;
            return FormToken(TokenKind::OP_PLUS, TokenStart);
        case '%': CurPtr++; return FormToken(TokenKind::OP_PERCENT, TokenStart);
        case '.':
            if(*(CurPtr + 1)=='.' && *(CurPtr + 2)=='=') {
              CurPtr+=3;
              return FormToken(TokenKind::OP_DOTDOTEQUAL, TokenStart);
            }
            else if(*(CurPtr + 1)== '.') {
              CurPtr += 2;
              return FormToken(TokenKind::OP_DOTDOT, TokenStart);
            }
            CurPtr++;
            return FormToken(TokenKind::DE_DOT, TokenStart);
        case '=': 
            if (*(CurPtr + 1) == '=') {
                CurPtr += 2;
                return FormToken(TokenKind::OP_EQUALEQUAL, TokenStart);
            }
            CurPtr++; 
            return FormToken(TokenKind::OP_EQUAL, TokenStart);
        case '*': CurPtr++; return FormToken(TokenKind::OP_STAR, TokenStart);
        case '<':
            if (*(CurPtr + 1) == '=') {
                CurPtr += 2;
                return FormToken(TokenKind::OP_LESSEQUAL, TokenStart);
            }
            else if(*(CurPtr + 1) == '<') {
              CurPtr += 2;
              return FormToken(TokenKind::OP_LESSLESS, TokenStart);
            }
            CurPtr++; 
            return FormToken(TokenKind::OP_LESS, TokenStart);
        case '>':
            if (*(CurPtr + 1) == '=') {
                CurPtr += 2;
                return FormToken(TokenKind::OP_GREATEREQUAL, TokenStart);
            }
            else if(*(CurPtr + 1) == '>') {
              CurPtr += 2;
              return FormToken(TokenKind::OP_GREATERGREATER, TokenStart);
            }
            CurPtr++; 
            return FormToken(TokenKind::OP_GREATER, TokenStart);
        case '!':
            if (*(CurPtr + 1) == '=') {
                CurPtr += 2;
                return FormToken(TokenKind::OP_BANGEQUAL, TokenStart);
            }
            CurPtr++;
            return FormToken(TokenKind::OP_BANG, TokenStart);
        case '&':
            if (*(CurPtr + 1) == '&') {
                CurPtr += 2;
                return FormToken(TokenKind::OP_AMPAMP, TokenStart);
            }
            CurPtr++;
            return FormToken(TokenKind::OP_AMP, TokenStart); 
        case '|':
            if (*(CurPtr + 1) == '|') {
                CurPtr += 2;
                return FormToken(TokenKind::OP_PIPEPIPE, TokenStart);
            }
            CurPtr++;
            return FormToken(TokenKind::OP_PIPE, TokenStart); 
        case '/': CurPtr++; 
            if (*(CurPtr) == '/') {
              SkipLineComment();
              return Lex();
            }
            return FormToken(TokenKind::OP_SLASH, TokenStart);
        default:
            CurPtr++;
            return FormToken(TokenKind::UNKNOWN, TokenStart);
      }
    }

    Token Lexer::FormToken(TokenKind Kind, const char* TokenStart) {
      unsigned Length = CurPtr - TokenStart;
      std::string_view Text(TokenStart, Length);
      SourceLocation Loc = SM.getLocation(TokenStart);
      return Token(Kind, Loc, Length, Text);
    }

    void Lexer::SkipWhiteSpace() {
      while (CurPtr < BufferEnd && isspace(*CurPtr)) {
        CurPtr++;
      }
    }

    void Lexer::SkipLineComment() {
      CurPtr+=2;
      while (CurPtr < BufferEnd && *CurPtr != '\n' && *CurPtr != '\r') {
        CurPtr++;
      }
    }

    Token Lexer::LexIdentifierOrKeyword(const char* TokenStart) {
      while (CurPtr < BufferEnd && (isalnum(*CurPtr) || *CurPtr == '_')) {
        CurPtr++;
      }
      TokenKind Kind;
      std::string_view Text(TokenStart, CurPtr - TokenStart);

      if (Text == "fn") Kind = TokenKind::KW_FN;
      else if (Text == "let") Kind = TokenKind::KW_LET;
      else if (Text == "mut") Kind = TokenKind::KW_MUT;
      else if (Text == "const") Kind = TokenKind::KW_CONST;
      else if (Text == "as") Kind = TokenKind::KW_AS;
      else if (Text == "if") Kind = TokenKind::KW_IF;
      else if (Text == "else") Kind = TokenKind::KW_ELSE;
      else if (Text == "return") Kind = TokenKind::KW_RETURN;
      else if (Text == "true") Kind = TokenKind::KW_TRUE;
      else if (Text == "false") Kind = TokenKind::KW_FALSE;
      else if (Text == "while") Kind = TokenKind::KW_WHILE;
      else if (Text == "for") Kind = TokenKind::KW_FOR;
      else if (Text == "in") Kind = TokenKind::KW_IN;
      else Kind = TokenKind::IDENTIFIER;

      return(FormToken(Kind, TokenStart));
    }


    Token Lexer::LexNumber(const char* TokenStart) {
      while (CurPtr < BufferEnd && isdigit(*CurPtr)) {
        CurPtr++;
      }

      if (*CurPtr == '.') {
        CurPtr++;
        if (*CurPtr == '.') {
          CurPtr--;
          return FormToken(TokenKind::LT_INTEGER, TokenStart);
        }
        else 
        {
          while (CurPtr < BufferEnd && isdigit(*CurPtr)) {
            CurPtr++;
          }
          return FormToken(TokenKind::LT_FLOAT, TokenStart);
        }
      }

      return FormToken(TokenKind::LT_INTEGER, TokenStart);
    }

    Token Lexer::LexStringLiteral(const char* TokenStart) {
      CurPtr++;
      while (CurPtr < BufferEnd && *CurPtr != '\"' ) {
        CurPtr++;
      }
      if (CurPtr < BufferEnd) {
        CurPtr++;
      }
      else {
        Diag.Report(DiagKind::Error, "Pointer out of buffer."); 
      }

      return FormToken(TokenKind::LT_STRING, TokenStart);

    }

  }

}



