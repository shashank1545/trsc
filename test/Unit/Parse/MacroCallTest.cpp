#include "trsc/Parse/Parser.h"

#include "trsc/AST/AST.h"
#include "trsc/AST/ASTContext.h"
#include "trsc/Basic/Diagnostics.h"
#include "trsc/Basic/IdentifierTable.h"
#include "trsc/Basic/SourceManager.h"
#include "trsc/Lex/Lexer.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

using namespace trsc;

namespace {

// Parses a whole program and hands back the first MacroCall in main's body,
// so the tests can assert on what the parser stored rather than on printed
// output.
class MacroCallTest : public ::testing::Test {
protected:
  DiagnosticsEngine Diag;
  SourceManager SM;
  IdentifierTable Idents;
  ASTContext Ctx;

  MacroCallTest() : Diag(), SM(Diag) {}

  Program *parse(const std::string &Source) {
    SM.loadBuffer(Source);
    Lex::Lexer Lexer(SM, Diag, Idents);

    Lex::Token Tok;
    do {
      Tok = Lexer.Lex();
      Tokens.push_back(Tok);
    } while (Tok.getKind() != Lex::TokenKind::ENDOFFILE);

    Parser P(Ctx, Diag, Tokens);
    return P.parse();
  }

  // The frontend is LLVM-free, so this walks node kinds directly rather than
  // using llvm::dyn_cast.
  MacroCall *firstMacroCallIn(Program *Prog) {
    if (!Prog)
      return nullptr;
    for (Stmt *Top : Prog->getStatements()) {
      if (!Top || Top->getASTNodeKind() != ASTNodeKind::ASTK_FUNCDECL)
        continue;
      Stmt *Body = static_cast<FuncDecl *>(Top)->getBody();
      if (!Body || Body->getASTNodeKind() != ASTNodeKind::ASTK_BLOCKSTMT)
        continue;
      for (Stmt *S : static_cast<BlockStmt *>(Body)->getStatements()) {
        if (!S || S->getASTNodeKind() != ASTNodeKind::ASTK_EXPRSTMT)
          continue;
        Expr *E = static_cast<ExprStmt *>(S)->getExpression();
        if (E && E->getASTNodeKind() == ASTNodeKind::ASTK_MACROCALL)
          return static_cast<MacroCall *>(E);
      }
    }
    return nullptr;
  }

private:
  std::vector<Lex::Token> Tokens;
};

TEST_F(MacroCallTest, ParsesNameFormatAndArguments) {
  Program *Prog =
      parse("fn main() { let a: i32 = 1; println!(\"x {} y\", a); }");
  ASSERT_EQ(Diag.getNumErrors(), 0u);

  MacroCall *Macro = firstMacroCallIn(Prog);
  ASSERT_NE(Macro, nullptr);
  EXPECT_EQ(Macro->getName(), "println");
  EXPECT_TRUE(Macro->hasFormatString());
  EXPECT_EQ(Macro->getFormatString(), "x {} y");
  EXPECT_EQ(Macro->getParams().size(), 1u);
}

TEST_F(MacroCallTest, ParsesEmptyInvocation) {
  Program *Prog = parse("fn main() { println!(); }");
  ASSERT_EQ(Diag.getNumErrors(), 0u);

  MacroCall *Macro = firstMacroCallIn(Prog);
  ASSERT_NE(Macro, nullptr);
  EXPECT_FALSE(Macro->hasFormatString());
  EXPECT_TRUE(Macro->getParams().empty());
}

// The parser resolves escapes so that Sema and MLIRGen only ever see runtime
// bytes; nothing downstream re-implements escape handling.
TEST_F(MacroCallTest, DecodesEscapeSequences) {
  Program *Prog = parse("fn main() { println!(\"a\\nb\\tc\\\\d\\\"e\"); }");
  ASSERT_EQ(Diag.getNumErrors(), 0u);

  MacroCall *Macro = firstMacroCallIn(Prog);
  ASSERT_NE(Macro, nullptr);
  EXPECT_EQ(Macro->getFormatString(), "a\nb\tc\\d\"e");
}

// Brace escapes are format-level, not lexical: they must survive the parser
// untouched so the format checker can count placeholders correctly.
TEST_F(MacroCallTest, LeavesBraceEscapesForTheFormatChecker) {
  Program *Prog = parse("fn main() { println!(\"{{}} {{{}}}\", 1); }");
  ASSERT_EQ(Diag.getNumErrors(), 0u);

  MacroCall *Macro = firstMacroCallIn(Prog);
  ASSERT_NE(Macro, nullptr);
  EXPECT_EQ(Macro->getFormatString(), "{{}} {{{}}}");
  EXPECT_EQ(Macro->getParams().size(), 1u);
}

TEST_F(MacroCallTest, ParsesArgumentExpressions) {
  Program *Prog =
      parse("fn main() { let a: i32 = 1; println!(\"{} {}\", a + 2, a); }");
  ASSERT_EQ(Diag.getNumErrors(), 0u);

  MacroCall *Macro = firstMacroCallIn(Prog);
  ASSERT_NE(Macro, nullptr);
  ASSERT_EQ(Macro->getParams().size(), 2u);
  EXPECT_EQ(Macro->getParams()[0]->getASTNodeKind(), ASTNodeKind::ASTK_BINEXPR);
  EXPECT_EQ(Macro->getParams()[1]->getASTNodeKind(), ASTNodeKind::ASTK_VAREXPR);
}

TEST_F(MacroCallTest, MissingClosingParenIsReportedNotHung) {
  parse("fn main() { let a: i32 = 1; println!(\"{}\", a }");
  EXPECT_GT(Diag.getNumErrors(), 0u);
}

TEST_F(MacroCallTest, UnterminatedFormatStringIsReported) {
  parse("fn main() { println!(\"oops); }");
  EXPECT_GT(Diag.getNumErrors(), 0u);
}

} // namespace
