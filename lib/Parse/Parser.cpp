#include "trsc/Parse/Parser.h"
#include "trsc/Basic/Diagnostics.h"

#include <charconv>
#include <cstdint>

namespace trsc {

namespace {
// std::from_chars on the token's view: no temporary std::string per literal.
template <typename T> T parseNumber(std::string_view Text) {
  T Value{};
  std::from_chars(Text.data(), Text.data() + Text.size(), Value);
  return Value;
}
} // namespace

Parser::Parser(ASTContext &Ctx, DiagnosticsEngine &Diag,
               const std::vector<Lex::Token> &Tokens)
    : Ctx(Ctx), Diag(Diag), Tokens(Tokens), CurrentTokenIdx(0) {}

int Parser::getOperatorPrecedence(Lex::TokenKind Kind) {
  switch (Kind) {
  case Lex::TokenKind::OP_PLUSEQUAL:
  case Lex::TokenKind::OP_MINUSEQUAL:
    return 17;
  case Lex::TokenKind::OP_BANG:
  case Lex::TokenKind::DE_DOT:
    return 15;
  case Lex::TokenKind::OP_STAR:
  case Lex::TokenKind::OP_SLASH:
  case Lex::TokenKind::OP_PERCENT:
    return 12;
  case Lex::TokenKind::OP_PLUS:
  case Lex::TokenKind::OP_MINUS:
    return 11;
  case Lex::TokenKind::OP_LESSLESS:
  case Lex::TokenKind::OP_GREATERGREATER:
    return 10;
  case Lex::TokenKind::OP_LESS:
  case Lex::TokenKind::OP_LESSEQUAL:
  case Lex::TokenKind::OP_GREATER:
  case Lex::TokenKind::OP_GREATEREQUAL:
    return 8;
  case Lex::TokenKind::OP_EQUALEQUAL:
  case Lex::TokenKind::OP_BANGEQUAL:
    return 7;
  case Lex::TokenKind::OP_AMP:
    return 6;
  case Lex::TokenKind::OP_PIPE:
    return 5;
  case Lex::TokenKind::OP_AMPAMP:
    return 4;
  case Lex::TokenKind::OP_PIPEPIPE:
    return 3;
  case Lex::TokenKind::OP_EQUAL:
    return 2;
  case Lex::TokenKind::IDENTIFIER:
  case Lex::TokenKind::LT_FLOAT:
  case Lex::TokenKind::LT_INTEGER:
    return 1;
  default:
    return -1;
  }
}

bool Parser::isAtEnd() const {
  return peek().getKind() == Lex::TokenKind::ENDOFFILE;
}

const Lex::Token &Parser::currentToken() const { return peek(0); }

void Parser::advance() {
  if (!isAtEnd()) {
    CurrentTokenIdx++;
  }
}

const Lex::Token &Parser::peek(unsigned Offset) const {
  if (CurrentTokenIdx + Offset >= Tokens.size()) {
    return Tokens.back();
  }
  return Tokens[CurrentTokenIdx + Offset];
}

bool Parser::match(std::initializer_list<Lex::TokenKind> Kinds) {
  for (Lex::TokenKind Kind : Kinds) {
    if (currentToken().getKind() == Kind) {
      advance();
      return true;
    }
  }
  return false;
}

Lex::Token Parser::consume(Lex::TokenKind ExpectedKind) {
  if (currentToken().getKind() == ExpectedKind) {
    const Lex::Token Consumed = currentToken();
    advance();
    return Consumed;
  }
  reportExpectedError(ExpectedKind);
  return currentToken();
}

void Parser::reportExpectedError(Lex::TokenKind ExpectedKind) {
  const Lex::Token &ActualToken = currentToken();
  std::string ExpectedStr = Lex::getTokenString(ExpectedKind);
  std::string ActualStr = Lex::getTokenString(ActualToken.getKind());

  std::string Msg = "Expected " + ExpectedStr + " but got " + ActualStr;
  Diag.Report(DiagKind::Error, Msg, ActualToken.getLocation());
}

bool Parser::expectToken(Lex::TokenKind Kind) {
  if (currentToken().getKind() != Kind) {
    reportExpectedError(Kind);
    return false;
  }
  consume(Kind);
  return true;
}

Program *Parser::parse() {
  SourceLocation Start = currentToken().getLocation();
  std::vector<Stmt *> Statements;
  while (!isAtEnd()) {
    Stmt *PStmt = parseStmt();
    if (PStmt) {
      Statements.push_back(PStmt);
    } else
      advance();
  }

  SourceLocation End = currentToken().getLocation();
  SourceRange LocRange = SourceRange(Start, End);
  return new (Ctx) Program(Ctx, LocRange, Statements);
}

Expr *Parser::parsePrimary() {
  SourceLocation StartLoc = currentToken().getLocation();
  SourceLocation EndLoc;
  SourceRange Range;
  switch (currentToken().getKind()) {
  case Lex::TokenKind::LT_FLOAT: {
    Lex::Token NumToken = consume(Lex::TokenKind::LT_FLOAT);
    double Val = parseNumber<double>(NumToken.getText());
    EndLoc = currentToken().getLocation();
    Range = SourceRange(StartLoc, EndLoc);
    return new (Ctx) FloatExpr(Val, Range);
  }
  case Lex::TokenKind::LT_INTEGER: {
    Lex::Token NumToken = consume(Lex::TokenKind::LT_INTEGER);
    int64_t Val = parseNumber<int64_t>(NumToken.getText());
    EndLoc = currentToken().getLocation();
    Range = SourceRange(StartLoc, EndLoc);
    return new (Ctx) IntExpr(Val, Range);
  }
  case Lex::TokenKind::KW_TRUE: {
    consume(Lex::TokenKind::KW_TRUE);
    EndLoc = currentToken().getLocation();
    Range = SourceRange(StartLoc, EndLoc);
    return new (Ctx) BoolExpr(true, Range);
  }
  case Lex::TokenKind::KW_FALSE: {
    consume(Lex::TokenKind::KW_FALSE);
    EndLoc = currentToken().getLocation();
    Range = SourceRange(StartLoc, EndLoc);
    return new (Ctx) BoolExpr(false, Range);
  }
  case Lex::TokenKind::IDENTIFIER: {
    Lex::Token IdentToken = consume(Lex::TokenKind::IDENTIFIER);
    if (currentToken().getKind() == Lex::TokenKind::DE_LPAREN) {
      return parseFunCall(IdentToken);
    } else if (currentToken().getKind() == Lex::TokenKind::DE_LBRACKET) {
      return parseArrayAccessExpr(IdentToken);
    } else {
      EndLoc = currentToken().getLocation();
      Range = SourceRange(StartLoc, EndLoc);
      return new (Ctx) VarExpr(IdentToken.getIdentifierInfo(), Range);
    }
  }
  case Lex::TokenKind::DE_LPAREN: {
    consume(Lex::TokenKind::DE_LPAREN);
    Expr *E = parseExpr(0);
    if (!E)
      return nullptr;
    if (currentToken().getKind() != Lex::TokenKind::DE_RPAREN) {
      reportExpectedError(Lex::TokenKind::DE_RPAREN);
      return nullptr;
    }
    consume(Lex::TokenKind::DE_RPAREN);
    return E;
  }
  case Lex::TokenKind::DE_LBRACKET: {
    std::vector<int64_t> Shape;
    return parseArray(Shape);
  }
  case Lex::TokenKind::OP_AMP: {
    consume(Lex::TokenKind::OP_AMP);
    bool IsMut = false;
    if (currentToken().getKind() == Lex::TokenKind::KW_MUT) {
      consume(Lex::TokenKind::KW_MUT);
      IsMut = true;
    }
    Expr *ReferentExpr = parsePrimary();
    EndLoc = currentToken().getLocation();
    Range = SourceRange(StartLoc, EndLoc);
    return new (Ctx) RefrExpr(ReferentExpr, IsMut, Range);
  }
  default:
    Diag.Report(DiagKind::Error, "Expected an expression",
                currentToken().getLocation());
    return nullptr;
  }
}

Expr *Parser::parseExpr(int MinPrecedence) {
  SourceLocation StartLoc = currentToken().getLocation();
  SourceLocation EndLoc;
  SourceRange Range;
  Expr *LHS = parsePrimary();
  if (!LHS)
    return nullptr;
  if (currentToken().getKind() == Lex::TokenKind::KW_AS) {
    consume(Lex::TokenKind::KW_AS);
    Type *ToType = parseType();
    EndLoc = currentToken().getLocation();
    Range = SourceRange(StartLoc, EndLoc);
    LHS = new (Ctx) ASExpr(LHS, ToType, Range);
  }
  while (true) {
    Lex::TokenKind CurrentOp = currentToken().getKind();
    int CurrentPrecedence = getOperatorPrecedence(CurrentOp);
    if (CurrentPrecedence < MinPrecedence)
      break;
    consume(CurrentOp);
    Expr *RHS = parseExpr(CurrentPrecedence + 1);
    if (!RHS)
      return nullptr;

    EndLoc = currentToken().getLocation();
    Range = SourceRange(StartLoc, EndLoc);
    LHS = new (Ctx) BinExpr(CurrentOp, LHS, RHS, Range);
  }
  return LHS;
}

// Case1: [elem; num]
// Case2: [elem, elem, .. elem]
// Mix: [[elem1;num1], [elem2;num2], [elem, elem... elem]];
ArrayExpr *Parser::parseArray(std::vector<int64_t> Shape) {
  consume(Lex::TokenKind::DE_LBRACKET);
  std::vector<Expr *> ChildElemExprVec;
  Expr *BaseExpr = parsePrimary();
  IntExpr *CountExpr = nullptr;
  ChildElemExprVec.push_back(BaseExpr);
  if (currentToken().getKind() == Lex::TokenKind::DE_SEMICOLON) {
    consume(Lex::TokenKind::DE_SEMICOLON);
    CountExpr = static_cast<IntExpr *>(parsePrimary());
    expectToken(Lex::TokenKind::DE_RBRACKET);
    Shape.push_back(CountExpr->getValue());
    return new (Ctx) ArrayExpr(Ctx, ChildElemExprVec, CountExpr, Shape);
  }
  while (currentToken().getKind() != Lex::TokenKind::DE_RBRACKET) {
    expectToken(Lex::TokenKind::DE_COMMA);
    ChildElemExprVec.push_back(parsePrimary());
  }
  consume(Lex::TokenKind::DE_RBRACKET);
  CountExpr = new (Ctx) IntExpr(static_cast<int64_t>(ChildElemExprVec.size()));
  Shape.push_back(CountExpr->getValue());
  return new (Ctx) ArrayExpr(Ctx, ChildElemExprVec, CountExpr, Shape);
}

ArrayAccessExpr *Parser::parseArrayAccessExpr(Lex::Token IndentToken) {
  std::vector<Expr *> IndexExprVec;
  while (true) {
    if (currentToken().getKind() != Lex::TokenKind::DE_LBRACKET)
      break;
    consume(Lex::TokenKind::DE_LBRACKET);
    IndexExprVec.push_back(parsePrimary());
    expectToken(Lex::TokenKind::DE_RBRACKET);
  }
  VarExpr *ArrayExprName = new (Ctx) VarExpr(IndentToken.getIdentifierInfo());
  return new (Ctx) ArrayAccessExpr(Ctx, ArrayExprName, IndexExprVec);
}

RangeExpr *Parser::parseRangeExpr() {
  SourceLocation StartLoc = currentToken().getLocation();
  Expr *Start = parsePrimary();
  bool IsInclusive;
  if (currentToken().getKind() == Lex::TokenKind::OP_DOTDOT) {
    IsInclusive = false;
    consume(Lex::TokenKind::OP_DOTDOT);
  } else if (currentToken().getKind() == Lex::TokenKind::OP_DOTDOTEQUAL) {
    IsInclusive = true;
    consume(Lex::TokenKind::OP_DOTDOTEQUAL);
  } else
    return nullptr;
  Expr *End = parsePrimary();
  SourceLocation EndLoc = currentToken().getLocation();
  SourceRange Range = SourceRange(StartLoc, EndLoc);
  return new (Ctx) RangeExpr(IsInclusive, Start, End, Range);
}

Stmt *Parser::parseStmt() {
  switch (currentToken().getKind()) {
  case Lex::TokenKind::KW_LET:
    return parseLetStmt();
  case Lex::TokenKind::KW_FOR:
    return parseForStmt();
  case Lex::TokenKind::KW_IF:
    return parseIfStmt();
  case Lex::TokenKind::KW_WHILE:
    return parseWhileStmt();
  case Lex::TokenKind::DE_LBRACE:
    return parseBlockStmt();
  case Lex::TokenKind::KW_FN:
    return parseFunction();
  case Lex::TokenKind::KW_RETURN:
    return parseReturnStmt();
  default:
    return parseExprStmt();
  }
}

LetStmt *Parser::parseLetStmt() {
  SourceLocation Start = currentToken().getLocation();
  SourceLocation End;
  SourceRange Range;
  consume(Lex::TokenKind::KW_LET);
  bool IsMut = false;
  if (currentToken().getKind() == Lex::TokenKind::KW_MUT) {
    consume(Lex::TokenKind::KW_MUT);
    IsMut = true;
  }
  if (currentToken().getKind() != Lex::TokenKind::IDENTIFIER) {
    reportExpectedError(Lex::TokenKind::IDENTIFIER);
    return nullptr;
  }
  VarExpr *DeclaredVar = static_cast<VarExpr *>(parsePrimary());
  Type *VarType = nullptr;
  if (currentToken().getKind() == Lex::TokenKind::DE_COLON) {
    consume(Lex::TokenKind::DE_COLON);
    VarType = parseType();
  }
  Expr *Initializer = nullptr;
  if (currentToken().getKind() == Lex::TokenKind::DE_SEMICOLON) {
    consume(Lex::TokenKind::DE_SEMICOLON);
    End = currentToken().getLocation();
    Range = SourceRange(Start, End);
  } else {
    if (!expectToken(Lex::TokenKind::OP_EQUAL))
      return nullptr;
    Initializer = parseExpr(0);
    if (!Initializer) {
      return nullptr;
    }
    if (currentToken().getKind() != Lex::TokenKind::DE_SEMICOLON) {
      reportExpectedError(Lex::TokenKind::DE_SEMICOLON);
      return nullptr;
    }
    consume(Lex::TokenKind::DE_SEMICOLON);
    End = currentToken().getLocation();
    Range = SourceRange(Start, End);
  }
  return new (Ctx) LetStmt(IsMut, DeclaredVar, VarType, Initializer, Range);
}

Type *Parser::parseType() {
  SourceLocation Start = currentToken().getLocation();
  SourceLocation End;
  SourceRange Range;
  if (currentToken().getKind() == Lex::TokenKind::OP_STAR) {
    consume(Lex::TokenKind::OP_STAR);
    bool IsMut;
    if (currentToken().getKind() == Lex::TokenKind::KW_MUT) {
      consume(Lex::TokenKind::KW_MUT);
      IsMut = true;
    } else if (currentToken().getKind() == Lex::TokenKind::KW_CONST) {
      consume(Lex::TokenKind::KW_CONST);
      IsMut = false;
    } else {
      Diag.Report(DiagKind::Error,
                  "Raw pointer types can only be mut or const.");
    }
    Type *Pointee = parseType();
    if (!Pointee)
      return nullptr;
    End = currentToken().getLocation();
    Range = SourceRange(Start, End);
    return new (Ctx) PointerTypeName(Pointee, IsMut, Range);
  } else if (currentToken().getKind() == Lex::TokenKind::OP_AMP) {
    consume(Lex::TokenKind::OP_AMP);
    bool IsMut;
    if (currentToken().is(Lex::TokenKind::KW_MUT)) {
      consume(Lex::TokenKind::KW_MUT);
      IsMut = true;
    } else {
      IsMut = false;
    }
    Type *Referent = parseType();
    if (!Referent)
      return nullptr;
    End = currentToken().getLocation();
    Range = SourceRange(Start, End);
    return new (Ctx) ReferenceTypeName(Referent, IsMut, Range);
  } else if (currentToken().getKind() == Lex::TokenKind::DE_LBRACKET) {
    consume(Lex::TokenKind::DE_LBRACKET);
    Type *Elemente = parseType();
    if (!Elemente)
      return nullptr;

    if (!expectToken(Lex::TokenKind::DE_SEMICOLON))
      return nullptr;

    if (currentToken().getKind() != Lex::TokenKind::LT_INTEGER) {
      Diag.Report(DiagKind::Error, "Expected array size",
                  currentToken().getLocation());
      return nullptr;
    }
    size_t Size = parseNumber<size_t>(currentToken().getText());
    consume(Lex::TokenKind::LT_INTEGER);
    if (!expectToken(Lex::TokenKind::DE_RBRACKET))
      return nullptr;
    End = currentToken().getLocation();
    Range = SourceRange(Start, End);
    return new (Ctx) ArrayTypeName(Elemente, Size, Range);
  } else {
    if (currentToken().getKind() != Lex::TokenKind::IDENTIFIER) {
      reportExpectedError(Lex::TokenKind::IDENTIFIER);
      return nullptr;
    }
    Lex::Token TypeNameToken = consume(Lex::TokenKind::IDENTIFIER);
    End = currentToken().getLocation();
    Range = SourceRange(Start, End);
    return new (Ctx) TypeName(TypeNameToken.getIdentifierInfo(), Range);
  }
}

ExprStmt *Parser::parseExprStmt() {
  SourceLocation Start = currentToken().getLocation();
  Expr *Expression = parseExpr(0);
  if (!Expression)
    return nullptr;

  SourceLocation End = currentToken().getLocation();
  SourceRange Range = SourceRange(Start, End);
  if (!expectToken(Lex::TokenKind::DE_SEMICOLON))
    return nullptr;

  return new (Ctx) ExprStmt(Range, Expression);
}

BlockStmt *Parser::parseBlockStmt() {
  SourceLocation Start = currentToken().getLocation();
  consume(Lex::TokenKind::DE_LBRACE);

  std::vector<Stmt *> Statements;
  while (currentToken().getKind() != Lex::TokenKind::DE_RBRACE && !isAtEnd()) {
    Stmt *S = parseStmt();
    if (S) {
      Statements.push_back(S);
    } else {
      while (currentToken().getKind() != Lex::TokenKind::DE_RBRACE &&
             currentToken().getKind() != Lex::TokenKind::DE_SEMICOLON &&
             !isAtEnd()) {
        advance();
      }
      if (currentToken().getKind() == Lex::TokenKind::DE_SEMICOLON)
        advance();
    }
  }

  SourceLocation End = currentToken().getLocation();
  if (!expectToken(Lex::TokenKind::DE_RBRACE))
    return nullptr;
  SourceRange Range = SourceRange(Start, End);
  return new (Ctx) BlockStmt(Ctx, Range, Statements);
}

IfStmt *Parser::parseIfStmt() {
  SourceLocation Start = currentToken().getLocation();
  consume(Lex::TokenKind::KW_IF);

  Expr *Condition = parseExpr(0);
  if (!Condition)
    return nullptr;

  Stmt *Then = parseStmt();
  if (!Then)
    return nullptr;

  Stmt *Else = nullptr;

  if (currentToken().getKind() == Lex::TokenKind::KW_ELSE) {
    consume(Lex::TokenKind::KW_ELSE);
    Else = parseStmt();
    if (!Else)
      return nullptr;
  }
  SourceLocation End = currentToken().getLocation();
  SourceRange Range = SourceRange(Start, End);

  return new (Ctx) IfStmt(Condition, Then, Else, Range);
}

ForStmt *Parser::parseForStmt() {
  SourceLocation Start = currentToken().getLocation();
  consume(Lex::TokenKind::KW_FOR);

  VarExpr *Init = static_cast<VarExpr *>(parsePrimary());
  if (!expectToken(Lex::TokenKind::KW_IN))
    return nullptr;

  RangeExpr *Range = parseRangeExpr();
  Stmt *Body = parseStmt();

  SourceLocation End = currentToken().getLocation();
  SourceRange RangeLoc = SourceRange(Start, End);
  return new (Ctx) ForStmt(Init, Range, Body, RangeLoc);
}

WhileStmt *Parser::parseWhileStmt() {
  SourceLocation Start = currentToken().getLocation();
  consume(Lex::TokenKind::KW_WHILE);
  Expr *Condition = parseExpr(0);
  Stmt *Block = parseStmt();
  SourceLocation End = currentToken().getLocation();
  SourceRange Range = SourceRange(Start, End);

  return new (Ctx) WhileStmt(Condition, Block, Range);
}

FuncDecl *Parser::parseFunction() {
  SourceLocation Start = currentToken().getLocation();
  consume(Lex::TokenKind::KW_FN);

  if (currentToken().getKind() != Lex::TokenKind::IDENTIFIER) {
    reportExpectedError(Lex::TokenKind::IDENTIFIER);
    return nullptr;
  }

  Lex::Token FuncNameToken = consume(Lex::TokenKind::IDENTIFIER);
  VarExpr *FuncName = new (Ctx) VarExpr(FuncNameToken.getIdentifierInfo());

  if (!expectToken(Lex::TokenKind::DE_LPAREN))
    return nullptr;

  std::vector<FuncDecl::Param> ParamVector;
  if (currentToken().getKind() != Lex::TokenKind::DE_RPAREN) {
    while (true) {
      FuncDecl::Param MyParam;
      if (currentToken().getKind() != Lex::TokenKind::IDENTIFIER) {
        reportExpectedError(Lex::TokenKind::IDENTIFIER);
        return nullptr;
      }
      MyParam.ParamName = static_cast<VarExpr *>(parsePrimary());
      if (currentToken().getKind() == Lex::TokenKind::DE_COLON) {
        consume(Lex::TokenKind::DE_COLON);
        MyParam.ParamType = parseType();
      }
      ParamVector.push_back(MyParam);

      if (currentToken().getKind() == Lex::TokenKind::DE_RPAREN)
        break;
      if (!expectToken(Lex::TokenKind::DE_COMMA))
        return nullptr;
    }
  }

  consume(Lex::TokenKind::DE_RPAREN);
  Type *FuncReturnType = nullptr;
  if (currentToken().getKind() == Lex::TokenKind::DE_RETURNTYPE) {
    consume(Lex::TokenKind::DE_RETURNTYPE);
    FuncReturnType = parseType();
  }
  BlockStmt *Block = parseBlockStmt();
  SourceLocation End = currentToken().getLocation();
  SourceRange LocRange = SourceRange(Start, End);

  return new (Ctx)
      FuncDecl(Ctx, LocRange, FuncName, FuncReturnType, ParamVector, Block);
}

FunCall *
Parser::parseFunCall(std::optional<Lex::Token> FuncNameToken = std::nullopt) {
  SourceLocation Start = currentToken().getLocation();
  if (!FuncNameToken) {
    if (currentToken().getKind() != Lex::TokenKind::IDENTIFIER) {
      reportExpectedError(Lex::TokenKind::IDENTIFIER);
    }
    FuncNameToken = currentToken();
    consume(Lex::TokenKind::IDENTIFIER);
  }
  VarExpr *FuncName = new (Ctx) VarExpr(FuncNameToken->getIdentifierInfo());
  if (!expectToken(Lex::TokenKind::DE_LPAREN))
    return nullptr;

  std::vector<Expr *> ParamVector;
  if (currentToken().getKind() != Lex::TokenKind::DE_RPAREN) {
    while (true) {
      ParamVector.push_back(parsePrimary());
      if (currentToken().getKind() == Lex::TokenKind::DE_RPAREN)
        break;
      if (!expectToken(Lex::TokenKind::DE_COMMA))
        return nullptr;
    }
  }

  consume(Lex::TokenKind::DE_RPAREN);
  SourceLocation End = currentToken().getLocation();
  SourceRange Range = SourceRange(Start, End);

  return new (Ctx) FunCall(Ctx, Range, FuncName, ParamVector);
}

ReturnStmt *Parser::parseReturnStmt() {
  SourceLocation Start = currentToken().getLocation();
  consume(Lex::TokenKind::KW_RETURN);

  Expr *ReturnValue = nullptr;
  if (currentToken().getKind() != Lex::TokenKind::DE_SEMICOLON) {
    ReturnValue = parseExpr(0);
  }

  SourceLocation End = currentToken().getLocation();
  if (!expectToken(Lex::TokenKind::DE_SEMICOLON))
    return nullptr;
  SourceRange Range = SourceRange(Start, End);

  return new (Ctx) ReturnStmt(Range, ReturnValue);
}

} // namespace trsc
