#include <gtest/gtest.h>
#include "trsc/Lex/Lexer.h"
#include "trsc/Basic/SourceManager.h"
#include "trsc/Basic/Diagnostics.h"

using namespace trsc::Lex;

class LexerTest : public ::testing::Test {
protected:
    trsc::SourceManager SM;
    trsc::DiagnosticsEngine Diag;

    LexerTest() : Diag(SM) {}
};

TEST_F(LexerTest, BasicTokens) {
    const std::string input = "let x = 10;";
    SM.setMainFileBuffer(input);
    Lexer lexer(SM, Diag);

    Token tok = lexer.Lex();
    EXPECT_EQ(tok.getKind(), TokenKind::kw_let);

    tok = lexer.Lex();
    EXPECT_EQ(tok.getKind(), TokenKind::identifier);
    EXPECT_EQ(tok.getIdentifier(), "x");

    tok = lexer.Lex();
    EXPECT_EQ(tok.getKind(), TokenKind::equal);

    tok = lexer.Lex();
    EXPECT_EQ(tok.getKind(), TokenKind::integer_literal);
    EXPECT_EQ(tok.getLiteralData(), "10");

    tok = lexer.Lex();
    EXPECT_EQ(tok.getKind(), TokenKind::semi);

    tok = lexer.Lex();
    EXPECT_EQ(tok.getKind(), TokenKind::eof);
}
