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

TEST_F(LexerTest, StringLiteralKeepsQuotesAndEscapes) {
  const std::string input = "\"plain\"";
  SM.loadBuffer(input);
  Lexer lexer(SM, Diag, Idents);

  Token tok = lexer.Lex();
  EXPECT_EQ(tok.getKind(), TokenKind::LT_STRING);
  EXPECT_EQ(tok.getText(), "\"plain\"");
  EXPECT_EQ(Diag.getNumErrors(), 0u);
}

// An escaped quote must not end the literal; before this was handled the
// lexer stopped at the backslash-quote and the rest of the line was lexed as
// stray identifiers.
TEST_F(LexerTest, StringLiteralEscapedQuoteDoesNotTerminate) {
  const std::string input = "\"say \\\"hi\\\" now\";";
  SM.loadBuffer(input);
  Lexer lexer(SM, Diag, Idents);

  Token tok = lexer.Lex();
  EXPECT_EQ(tok.getKind(), TokenKind::LT_STRING);
  EXPECT_EQ(tok.getText(), "\"say \\\"hi\\\" now\"");
  EXPECT_EQ(Diag.getNumErrors(), 0u);

  tok = lexer.Lex();
  EXPECT_EQ(tok.getKind(), TokenKind::DE_SEMICOLON);
}

TEST_F(LexerTest, StringLiteralEscapedBackslashBeforeQuote) {
  const std::string input = "\"trailing\\\\\"";
  SM.loadBuffer(input);
  Lexer lexer(SM, Diag, Idents);

  Token tok = lexer.Lex();
  EXPECT_EQ(tok.getKind(), TokenKind::LT_STRING);
  EXPECT_EQ(tok.getText(), "\"trailing\\\\\"");
  EXPECT_EQ(Diag.getNumErrors(), 0u);

  tok = lexer.Lex();
  EXPECT_EQ(tok.getKind(), TokenKind::ENDOFFILE);
}

TEST_F(LexerTest, UnterminatedStringLiteralIsReported) {
  const std::string input = "\"no end";
  SM.loadBuffer(input);
  Lexer lexer(SM, Diag, Idents);

  Token tok = lexer.Lex();
  EXPECT_EQ(tok.getKind(), TokenKind::LT_STRING);
  EXPECT_EQ(Diag.getNumErrors(), 1u);
}

TEST_F(LexerTest, UnknownEscapeSequenceIsReported) {
  const std::string input = "\"bad \\q escape\"";
  SM.loadBuffer(input);
  Lexer lexer(SM, Diag, Idents);

  Token tok = lexer.Lex();
  EXPECT_EQ(tok.getKind(), TokenKind::LT_STRING);
  EXPECT_EQ(Diag.getNumErrors(), 1u);
}

TEST_F(LexerTest, DecodeEscapeCoversSupportedSet) {
  EXPECT_EQ(Lexer::decodeEscape('n'), '\n');
  EXPECT_EQ(Lexer::decodeEscape('r'), '\r');
  EXPECT_EQ(Lexer::decodeEscape('t'), '\t');
  EXPECT_EQ(Lexer::decodeEscape('0'), '\0');
  EXPECT_EQ(Lexer::decodeEscape('\\'), '\\');
  EXPECT_EQ(Lexer::decodeEscape('\''), '\'');
  EXPECT_EQ(Lexer::decodeEscape('"'), '"');
  EXPECT_FALSE(Lexer::decodeEscape('q').has_value());
  EXPECT_FALSE(Lexer::decodeEscape('x').has_value());
}

TEST_F(LexerTest, MacroCallLexesAsIdentifierBang) {
  const std::string input = "println!(\"x {}\", a)";
  SM.loadBuffer(input);
  Lexer lexer(SM, Diag, Idents);

  Token tok = lexer.Lex();
  EXPECT_EQ(tok.getKind(), TokenKind::IDENTIFIER);
  EXPECT_EQ(tok.getText(), "println");

  tok = lexer.Lex();
  EXPECT_EQ(tok.getKind(), TokenKind::OP_BANG);

  tok = lexer.Lex();
  EXPECT_EQ(tok.getKind(), TokenKind::DE_LPAREN);

  tok = lexer.Lex();
  EXPECT_EQ(tok.getKind(), TokenKind::LT_STRING);
  EXPECT_EQ(tok.getText(), "\"x {}\"");
  EXPECT_EQ(Diag.getNumErrors(), 0u);
}
