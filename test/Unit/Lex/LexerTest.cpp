#include "trsc/Lex/Lexer.h"
#include "trsc/Basic/Diagnostics.h"
#include "trsc/Basic/IdentifierTable.h"
#include "trsc/Basic/SourceManager.h"
#include <gtest/gtest.h>

using namespace trsc::Lex;

class LexerTest : public ::testing::Test {
protected:
  trsc::SourceManager SM;
  trsc::DiagnosticsEngine Diag;
  trsc::IdentifierTable Idents;

  LexerTest() : SM(Diag), Diag() {}
};

TEST_F(LexerTest, BasicTokens) {
  const std::string input = "let x = 10;";
  SM.loadBuffer(input);
  Lexer lexer(SM, Diag, Idents);

  Token tok = lexer.Lex();
  EXPECT_EQ(tok.getKind(), TokenKind::KW_LET);

  tok = lexer.Lex();
  EXPECT_EQ(tok.getKind(), TokenKind::IDENTIFIER);
  EXPECT_EQ(tok.getText(), "x");

  tok = lexer.Lex();
  EXPECT_EQ(tok.getKind(), TokenKind::OP_EQUAL);

  tok = lexer.Lex();
  EXPECT_EQ(tok.getKind(), TokenKind::LT_INTEGER);
  EXPECT_EQ(tok.getText(), "10");

  tok = lexer.Lex();
  EXPECT_EQ(tok.getKind(), TokenKind::DE_SEMICOLON);

  tok = lexer.Lex();
  EXPECT_EQ(tok.getKind(), TokenKind::ENDOFFILE);
}
