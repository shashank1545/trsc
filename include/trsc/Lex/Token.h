#ifndef TRSC_LEX_TOKEN_H
#define TRSC_LEX_TOKEN_H

#include "trsc/Basic/SourceLocation.h"
#include <string_view>

namespace trsc {
  namespace Lex {
 
    enum class TokenKind { 
      UNKNOWN,
      ENDOFFILE,

      // KEYWORDS
      KW_FN,
      KW_LET,
      KW_MUT,
      KW_IF,
      KW_ELSE,
      KW_RETURN,
      KW_TRUE,
      KW_FALSE,
      KW_WHILE,
      KW_FOR,
      KW_IN,

      //OPERATORS
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
      OP_PIPEPIPE, 

      //DELIMITERS
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

      //LITERALS
      LT_INTEGER,
      LT_FLOAT,
      LT_STRING,

      //IDENTIFIER
      IDENTIFIER,
    };

    const char* getTokenName(TokenKind K); 
    const char* getTokenString(TokenKind K); 

    class Token {

      TokenKind Kind;
      SourceLocation Loc;
      unsigned Length;
      std::string_view Text;

      public:
        Token(): Kind(TokenKind::UNKNOWN), Loc({nullptr,0,0}), Length(0), Text("") {}

        Token(TokenKind K, SourceLocation L, unsigned Len, std::string_view T):
          Kind(K), Loc(L), Length(Len), Text(T) {}

        TokenKind getKind() const { return Kind; }
        SourceLocation getLocation() const { return Loc; }
        unsigned getLength() const { return Length; }
        std::string_view getText() const { return Text; }

        bool is(TokenKind K) const { return Kind == K;}
        bool isNot(TokenKind K) const { return Kind != K;}
        bool isIdentifier() const { return Kind == TokenKind::IDENTIFIER; }


        bool isKeyword() const {
          switch(Kind) {
            case TokenKind::KW_FN:
            case TokenKind::KW_LET:
            case TokenKind::KW_MUT:
            case TokenKind::KW_IF:
            case TokenKind::KW_ELSE:
            case TokenKind::KW_RETURN:
            case TokenKind::KW_TRUE:
            case TokenKind::KW_FALSE:
            case TokenKind::KW_WHILE:
            case TokenKind::KW_FOR:
            case TokenKind::KW_IN:
              return true;
            default:
              return false;
          }
        }

        bool isOperator() const {
          switch (Kind) {
            case TokenKind::OP_PLUS:
            case TokenKind::OP_MINUS:
            case TokenKind::OP_STAR:
            case TokenKind::OP_SLASH:
            case TokenKind::OP_PERCENT:
            case TokenKind::OP_EQUAL:
            case TokenKind::OP_EQUALEQUAL:
            case TokenKind::OP_BANG:
            case TokenKind::OP_BANGEQUAL:
            case TokenKind::OP_LESS:
            case TokenKind::OP_LESSEQUAL:
            case TokenKind::OP_GREATER:
            case TokenKind::OP_GREATEREQUAL:
            case TokenKind::OP_COLONCOLON:
            case TokenKind::OP_PLUSEQUAL:
            case TokenKind::OP_MINUSEQUAL:
            case TokenKind::OP_STARSTAR:
            case TokenKind::OP_LESSLESS:
            case TokenKind::OP_GREATERGREATER:
            case TokenKind::OP_DOTDOT:
            case TokenKind::OP_DOTDOTEQUAL:
              return true;
            default:
              return false;
          }
        }

        bool isLiteral() const {
          switch(Kind) {
            case TokenKind::LT_INTEGER:
            case TokenKind::LT_FLOAT:
            case TokenKind::LT_STRING:
              return true;
            default:
              return false;
          }
        }

    };

  }
}

#endif // TRSC_LEX_TOKEN_H
