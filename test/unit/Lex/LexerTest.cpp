#include <gtest/gtest.h>
#include "trsc/Lex/Lexer.h"
#include "trsc/Basic/SourceManager.h"
#include "trsc/Basic/Diagnostics.h"

using namespace trsc::Lex;

class LexerTest : public ::testing::Test {
protected:
    trsc::SourceManager SM;
    trsc::DiagnosticsEngine Diag;

    LexerTest() :  SM(Diag), Diag()  {}
};

TEST_F(LexerTest, BasicTokens) {
    const std::string input = "let x = 10;";
    SM.loadFile(input);
    Lexer lexer(SM, Diag);

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
