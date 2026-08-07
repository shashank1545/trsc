#ifndef TRSC_PARSE_PARSER_H
#define TRSC_PARSE_PARSER_H

#include "trsc/AST/AST.h"

#include <cstdint>
#include <optional>

namespace trsc {
class DiagnosticsEngine;
class Parser {
public:
  // Ctx owns the AST arena the parser allocates into; constructing a Parser
  // therefore requires the context to already exist and to outlive the tree.
  Parser(ASTContext &Ctx, DiagnosticsEngine &Diag,
         const std::vector<Lex::Token> &Tokens);
  Program *parse();

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

  bool decodeStringLiteral(const Lex::Token &Tok, std::string_view &Out);

  trsc::Type *parseType();
  trsc::Stmt *parseStmt();
  trsc::LetStmt *parseLetStmt();
  trsc::Expr *parseExpr(int);
  trsc::ArrayExpr *parseArray(std::vector<int64_t> Shape);
  trsc::ArrayAccessExpr *parseArrayAccessExpr(Lex::Token Token);
  trsc::Expr *parsePrimary();
  trsc::MacroCall *parseMacroCall(Lex::Token MacroNameToken);
  trsc::RangeExpr *parseRangeExpr();
  trsc::WhileStmt *parseWhileStmt();
  trsc::ForStmt *parseForStmt();
  trsc::IfStmt *parseIfStmt();
  trsc::BlockStmt *parseBlockStmt();
  trsc::ExprStmt *parseExprStmt();
  trsc::FuncDecl *parseFunction();
  trsc::ReturnStmt *parseReturnStmt();
  trsc::FunCall *parseFunCall(std::optional<Lex::Token>);
};
} // namespace trsc

#endif // TRSC_PARSE_PARSER_H
