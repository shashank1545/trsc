#ifndef TRSC_PARSE_PARSER_H
#define TRSC_PARSE_PARSER_H

#include "trsc/AST/AST.h"
#include "trsc/Lex/Token.h"
#include "trsc/Basic/Diagnostics.h"

#include <memory>
#include <vector>
#include <optional>
#include <initializer_list>

namespace trsc {
  class Parser {
    public:
      Parser(DiagnosticsEngine &Diag, const std::vector<Lex::Token> &Tokens);
      std::unique_ptr<Program> parse();

      int getOperatorPrecedence(Lex::TokenKind);
      const Lex::Token& currentToken() const;
      bool match(std::initializer_list<Lex::TokenKind> Kinds);
      Lex::Token consume(Lex::TokenKind ExpectedKind);

    private:
      DiagnosticsEngine &Diag;
      const std::vector<Lex::Token> &Tokens;
      unsigned CurrentTokenIdx;

      bool isAtEnd() const;
      void advance();
      const Lex::Token& peek(unsigned Offset = 0) const;
      bool expectToken(Lex::TokenKind Kind);
      void reportExpectedError(Lex::TokenKind ExpectedKind);

      std::unique_ptr<trsc::Type> parseType();
      std::unique_ptr<trsc::Stmt> parseStmt();
      std::unique_ptr<trsc::LetStmt> parseLetStmt();
      std::unique_ptr<trsc::Expr> parseExpr(int);
      std::unique_ptr<trsc::Expr> parsePrimary();
      std::unique_ptr<trsc::BinExpr> parseBinExpr();
      std::unique_ptr<trsc::RangeExpr> parseRangeExpr();
      std::unique_ptr<trsc::WhileStmt> parseWhileStmt();
      std::unique_ptr<trsc::ForStmt> parseForStmt();
      std::unique_ptr<trsc::IfStmt> parseIfStmt();
      std::unique_ptr<trsc::IfStmt> parseIfPrim();
      std::unique_ptr<trsc::BlockStmt> parseBlockStmt();
      std::unique_ptr<trsc::ExprStmt> parseExprStmt();
      std::unique_ptr<trsc::FuncDecl> parseFunction();
      std::unique_ptr<trsc::ReturnStmt> parseReturnStmt();
      std::unique_ptr<trsc::FunCall> parseFunCall(std::optional<Lex::Token>);
  };
}

#endif // TRSC_PARSE_PARSER_H
