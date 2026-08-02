#ifndef TRSC_PARSE_PARSER_H
#define TRSC_PARSE_PARSER_H

#include "trsc/AST/AST.h"

#include <memory>
#include <optional>

namespace trsc {
class ASTContext;
class DiagnosticsEngine;
class Parser {
public:
  // Ctx owns the AST arena the parser allocates into; constructing a Parser
  // therefore requires the context to already exist and to outlive the tree.
  Parser(ASTContext &Ctx, DiagnosticsEngine &Diag,
         const std::vector<Lex::Token> &Tokens);
  std::unique_ptr<Program> parse();

private:
  ASTContext &Ctx;
  DiagnosticsEngine &Diag;
  const std::vector<Lex::Token> &Tokens;
  unsigned CurrentTokenIdx;

  int getOperatorPrecedence(Lex::TokenKind);
  const Lex::Token &currentToken() const;
  bool match(std::initializer_list<Lex::TokenKind> Kinds);
  Lex::Token consume(Lex::TokenKind ExpectedKind);

  bool isAtEnd() const;
  void advance();
  const Lex::Token &peek(unsigned Offset = 0) const;
  bool expectToken(Lex::TokenKind Kind);
  void reportExpectedError(Lex::TokenKind ExpectedKind);

  std::unique_ptr<trsc::Type> parseType();
  std::unique_ptr<trsc::Stmt> parseStmt();
  std::unique_ptr<trsc::LetStmt> parseLetStmt();
  std::unique_ptr<trsc::Expr> parseExpr(int);
  std::unique_ptr<trsc::ArrayExpr> parseArray(std::vector<int>);
  std::unique_ptr<trsc::ArrayAccessExpr> parseArrayAccessExpr(Lex::Token Token);
  std::unique_ptr<trsc::Expr> parsePrimary();
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
} // namespace trsc

#endif // TRSC_PARSE_PARSER_H
