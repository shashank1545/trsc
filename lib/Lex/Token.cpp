#include "trsc/Lex/Token.h"

namespace trsc {
  namespace Lex {

    const char* getTokenName(TokenKind K) {
      switch(K) {
#define TOK(ID,NAME) case TokenKind::ID: return NAME

        TOK(UNKNOWN,"UNKNOWN");
        TOK(ENDOFFILE,"ENDOFFILE");

        // KEYWORDS
        TOK(KW_FN,"KW_FN");
        TOK(KW_LET,"KW_LET");
        TOK(KW_MUT,"KW_MUT");
        TOK(KW_IF,"KW_IF");
        TOK(KW_ELSE,"KW_ELSE");
        TOK(KW_RETURN,"KW_RETURN");
        TOK(KW_TRUE,"KW_TRUE");
        TOK(KW_FALSE,"KW_FALSE");
        TOK(KW_WHILE,"KW_WHILE");
        TOK(KW_FOR,"KW_FOR");
        TOK(KW_IN,"KW_IN");

        //OPERATORS
        TOK(OP_PLUS,"OP_PLUS");
        TOK(OP_MINUS,"OP_MINUS");
        TOK(OP_STAR,"OP_STAR");
        TOK(OP_SLASH,"OP_SLASH");
        TOK(OP_PERCENT,"OP_PERCENT");
        TOK(OP_EQUAL,"OP_EQUAL");
        TOK(OP_EQUALEQUAL,"OP_EQUALEQUAL");
        TOK(OP_BANG,"OP_BANG");
        TOK(OP_BANGEQUAL, "OP_BANGEQUAL");
        TOK(OP_LESS, "OP_LESS");
        TOK(OP_LESSEQUAL,"OP_LESSEQUAL");
        TOK(OP_GREATER,"OP_GREATER");
        TOK(OP_GREATEREQUAL,"OP_GREATEREQUAL");
        TOK(OP_COLONCOLON,"OP_COLONCOLON");
        TOK(OP_PLUSEQUAL,"OP_PLUSEQUAL");
        TOK(OP_MINUSEQUAL,"OP_MINUSEQUAL");
        TOK(OP_STARSTAR,"OP_STARSTAR");
        TOK(OP_LESSLESS,"OP_LESSLESS");
        TOK(OP_GREATERGREATER,"OP_GREATERGREATER");
        TOK(OP_DOTDOT,"OP_DOTDOT");
        TOK(OP_DOTDOTEQUAL,"OP_DOTDOTEQUAL");
        TOK(OP_AMP,"OP_AMP");
        TOK(OP_AMPAMP,"OP_AMPAMP");
        TOK(OP_PIPEPIPE,"OP_PIPEPIPE");

        //DELIMITERS
        TOK(DE_LPAREN,"DE_LPAREN");
        TOK(DE_RPAREN,"DE_RPAREN");
        TOK(DE_LBRACE,"DE_LBRACE");
        TOK(DE_RBRACE,"DE_RBRACE");
        TOK(DE_LBRACKET,"DE_LBRACKET");
        TOK(DE_RBRACKET,"DE_RBRACKET");
        TOK(DE_COMMA,"DE_COMMA");
        TOK(DE_SEMICOLON,"DE_SEMICOLON");
        TOK(DE_COLON,"DE_COLON");
        TOK(DE_DOT,"DE_DOT");
        TOK(DE_RETURNTYPE,"DE_RETURNTYPE");

        //LITERALS
        TOK(LT_INTEGER,"LT_INTEGER");
        TOK(LT_FLOAT,"LT_FLOAT");
        TOK(LT_STRING,"LT_STRING");

        //IDENTIFIER
        TOK(IDENTIFIER,"IDENTIFIER");
      }
    }

    const char* getTokenString(TokenKind K) {
      switch(K) {
#define TOK(ID,NAME) case TokenKind::ID: return NAME

        TOK(UNKNOWN,"unknown");
        TOK(ENDOFFILE,"endoffile");

        // KEYWORDS
        TOK(KW_FN,"fn");
        TOK(KW_LET,"let");
        TOK(KW_MUT,"mut");
        TOK(KW_IF,"if");
        TOK(KW_ELSE,"else");
        TOK(KW_RETURN,"return");
        TOK(KW_TRUE,"true");
        TOK(KW_FALSE,"false");
        TOK(KW_WHILE,"while");
        TOK(KW_FOR,"for");
        TOK(KW_IN,"in");

        //OPERATORS
        TOK(OP_PLUS,"+");
        TOK(OP_MINUS,"-");
        TOK(OP_STAR,"*");
        TOK(OP_SLASH,"/");
        TOK(OP_PERCENT,"%");
        TOK(OP_EQUAL,"=");
        TOK(OP_EQUALEQUAL,"==");
        TOK(OP_BANG,"!");
        TOK(OP_BANGEQUAL, "!=");
        TOK(OP_LESS, "<");
        TOK(OP_LESSEQUAL,"<=");
        TOK(OP_GREATER,">");
        TOK(OP_GREATEREQUAL,">=");
        TOK(OP_COLONCOLON,"::");
        TOK(OP_PLUSEQUAL,"+=");
        TOK(OP_MINUSEQUAL,"-=");
        TOK(OP_STARSTAR,"**");
        TOK(OP_LESSLESS,"<<");
        TOK(OP_GREATERGREATER,">>");
        TOK(OP_DOTDOT,"..");
        TOK(OP_DOTDOTEQUAL,"..=");
        TOK(OP_AMP,"&");
        TOK(OP_AMPAMP,"&&");
        TOK(OP_PIPEPIPE,"||");

        //DELIMITERS
        TOK(DE_LPAREN,"{");
        TOK(DE_RPAREN,"}");
        TOK(DE_LBRACE,"(");
        TOK(DE_RBRACE,")");
        TOK(DE_LBRACKET,"[");
        TOK(DE_RBRACKET,"]");
        TOK(DE_COMMA,",");
        TOK(DE_SEMICOLON,";");
        TOK(DE_COLON,":");
        TOK(DE_DOT,".");
        TOK(DE_RETURNTYPE,"->");

        //LITERALS
        TOK(LT_INTEGER,"number");
        TOK(LT_FLOAT,"number");
        TOK(LT_STRING,"string");

        //IDENTIFIER
        TOK(IDENTIFIER,"identifier");
      }
    }
  } // namespace Lex
} // namespace trsc
