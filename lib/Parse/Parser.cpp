#include "trsc/Parse/Parser.h"
#include "trsc/Basic/SourceLocation.h"

#include <string>

namespace trsc {

  Parser::Parser(DiagnosticsEngine &Diag, const std::vector<Lex::Token> &Tokens)
    : Diag(Diag), Tokens(Tokens), CurrentTokenIdx(0) {}

  int Parser::getOperatorPrecedence(Lex::TokenKind Kind) {
    switch(Kind) {
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

  const Lex::Token& Parser::currentToken() const {
    return peek(0);
  }

  void Parser::advance() {
    if(!isAtEnd()) {
      CurrentTokenIdx++;
    }
  }

  const Lex::Token& Parser::peek(unsigned Offset) const {
    if (CurrentTokenIdx + Offset >= Tokens.size()) {
      return Tokens.back();
    }
    return Tokens[CurrentTokenIdx + Offset];
  }
  
  bool Parser::match(std::initializer_list<Lex::TokenKind> Kinds) {
    for (Lex::TokenKind Kind: Kinds) {
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
    const Lex::Token& ActualToken = currentToken();
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

  std::unique_ptr<Program> Parser::parse() {
    SourceLocation Start = currentToken().getLocation();
    std::vector<std::unique_ptr<Stmt>> Statements;
    while (!isAtEnd()) {
      std::unique_ptr<trsc::Stmt> PStmt = parseStmt();
      if (PStmt) { 
        Statements.push_back(std::move(PStmt));
      } else advance();
    }

    SourceLocation End = currentToken().getLocation();
    SourceRange LocRange = SourceRange(Start, End);
    return std::make_unique<Program>(LocRange, std::move(Statements));
  }

  std::unique_ptr<Expr> Parser::parsePrimary() {
    SourceLocation StartLoc = currentToken().getLocation();
    SourceLocation EndLoc;
    SourceRange Range;
    switch(currentToken().getKind()) {
      case Lex::TokenKind::LT_FLOAT: {
        Lex::Token NumToken = consume(Lex::TokenKind::LT_FLOAT);
        double Val = std::stod(std::string(NumToken.getText()));
        EndLoc = currentToken().getLocation();
        Range = SourceRange(StartLoc, EndLoc);
        return std::make_unique<FloatExpr>(Val, Range);
      }
      case Lex::TokenKind::LT_INTEGER: {
        Lex::Token NumToken = consume(Lex::TokenKind::LT_INTEGER);
        long long Val = std::stoll(std::string(NumToken.getText()));
        EndLoc = currentToken().getLocation();
        Range = SourceRange(StartLoc, EndLoc);
        return std::make_unique<IntExpr>(Val, Range);
      }
      case Lex::TokenKind::KW_TRUE: {
        consume(Lex::TokenKind::KW_TRUE);
        EndLoc = currentToken().getLocation();
        Range = SourceRange(StartLoc, EndLoc);
        return std::make_unique<BoolExpr>(true, Range);
      }
      case Lex::TokenKind::KW_FALSE: {
        consume(Lex::TokenKind::KW_FALSE);
        EndLoc = currentToken().getLocation();
        Range = SourceRange(StartLoc, EndLoc);
        return std::make_unique<BoolExpr>(false, Range);
      }
      case Lex::TokenKind::IDENTIFIER: {
        Lex::Token IdentToken = consume(Lex::TokenKind::IDENTIFIER);
        if(currentToken().getKind() == Lex::TokenKind::DE_LPAREN) {
          return parseFunCall(IdentToken);
        }
        else {
          EndLoc = currentToken().getLocation();
          Range = SourceRange(StartLoc, EndLoc);
          return std::make_unique<VarExpr>(std::string(IdentToken.getText()),
              Range);
        }
      }
      case Lex::TokenKind::DE_LPAREN: {
        consume(Lex::TokenKind::DE_LPAREN);
        auto E = parseExpr(0);
        if (!E) return nullptr;
        if (currentToken().getKind() != Lex::TokenKind::DE_RPAREN) {
          reportExpectedError(Lex::TokenKind::DE_RPAREN);
          return nullptr;
        }
        consume(Lex::TokenKind::DE_RPAREN);
        return E;
      }
      case Lex::TokenKind::OP_AMP: {
        consume(Lex::TokenKind::OP_AMP);
        bool IsMut;
        if(currentToken().getKind() == Lex::TokenKind::KW_MUT){
          consume(Lex::TokenKind::KW_MUT);
          IsMut = true;
        } else {
          IsMut = false;
        }
        std::unique_ptr<Expr> ReferentExpr = parsePrimary();
        EndLoc = currentToken().getLocation();
        Range = SourceRange(StartLoc, EndLoc);
        return std::make_unique<RefrExpr>(std::move(ReferentExpr), IsMut, Range);
      }
      default:
        Diag.Report(DiagKind::Error, "Expected an expression", 
            currentToken().getLocation());
        return nullptr;
    } 
  }

  std::unique_ptr<Expr> Parser::parseExpr(int MinPrecedence) {
    SourceLocation StartLoc = currentToken().getLocation();
    SourceLocation EndLoc;
    SourceRange Range;
    auto LHS = parsePrimary();
    if(!LHS) return nullptr;
    std::unique_ptr<Type> ToType;
    if(currentToken().getKind() == Lex::TokenKind::KW_AS) {
      consume(Lex::TokenKind::KW_AS);
      ToType = parseType();
      EndLoc = currentToken().getLocation();
      Range = SourceRange(StartLoc, EndLoc);
      LHS = std::make_unique<ASExpr>(std::move(LHS), std::move(ToType), Range);
    }
    while(true) {
      Lex::TokenKind CurrentOp = currentToken().getKind();
      int CurrentPrecedence = getOperatorPrecedence(CurrentOp);
      
      if (CurrentPrecedence < MinPrecedence) break;

      consume(CurrentOp);

      auto RHS = parseExpr(CurrentPrecedence + 1);
      if(!RHS) return nullptr;

      EndLoc = currentToken().getLocation();
      Range = SourceRange(StartLoc, EndLoc);
      LHS= std::make_unique<BinExpr>(CurrentOp, std::move(LHS), std::move(RHS),
          Range);
    }
    return LHS;
  }

  std::unique_ptr<RangeExpr> Parser::parseRangeExpr() {
    SourceLocation StartLoc = currentToken().getLocation();
    std::unique_ptr<Expr> Start = parsePrimary();
    bool IsInclusive ;
    if(currentToken().getKind() == Lex::TokenKind::OP_DOTDOT) {
      IsInclusive = false;
      consume(Lex::TokenKind::OP_DOTDOT);
    }
    else if(currentToken().getKind() == Lex::TokenKind::OP_DOTDOTEQUAL) {
      IsInclusive = true;
      consume(Lex::TokenKind::OP_DOTDOTEQUAL);
    }
    else return nullptr;
    std::unique_ptr<Expr> End = parsePrimary();
    SourceLocation EndLoc = currentToken().getLocation();
    SourceRange Range = SourceRange(StartLoc, EndLoc);
    return std::make_unique<RangeExpr>(IsInclusive, std::move(Start), 
        std::move(End), Range); 
  }

  std::unique_ptr<Stmt> Parser::parseStmt() {
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

  std::unique_ptr<LetStmt> Parser::parseLetStmt() {
    SourceLocation Start = currentToken().getLocation();
    SourceLocation End;
    SourceRange Range;
    consume(Lex::TokenKind::KW_LET);
    bool IsMut = false;
    if(currentToken().getKind() == Lex::TokenKind::KW_MUT){
      consume(Lex::TokenKind::KW_MUT);
      IsMut = true;
    }
    if(currentToken().getKind() != Lex::TokenKind::IDENTIFIER) {
      reportExpectedError(Lex::TokenKind::IDENTIFIER);
      return nullptr;
    }
    auto DeclRawPtr = parsePrimary().release();
    std::unique_ptr<VarExpr> DeclaredVar(static_cast<VarExpr*>(DeclRawPtr));
    std::unique_ptr<Type> VarType; 
    if(currentToken().getKind() == Lex::TokenKind::DE_COLON) {
      consume(Lex::TokenKind::DE_COLON);
      VarType = parseType();
    }
    std::unique_ptr<Expr> Initializer;
    if(currentToken().getKind() == Lex::TokenKind::DE_SEMICOLON) {
      consume(Lex::TokenKind::DE_SEMICOLON);
      End = currentToken().getLocation();
      Range = SourceRange(Start, End);
    } else {
      if(!expectToken(Lex::TokenKind::OP_EQUAL)) return nullptr;
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
    return std::make_unique<trsc::LetStmt>(IsMut, std::move(DeclaredVar), 
        std::move(VarType), std::move(Initializer), Range);
  }


  std::unique_ptr<Type> Parser::parseType() {
    SourceLocation Start = currentToken().getLocation();
    SourceLocation End;
    SourceRange Range;
    if(currentToken().getKind() == Lex::TokenKind::OP_STAR) {
      consume(Lex::TokenKind::OP_STAR);
      bool IsMut;
      if(currentToken().getKind() == Lex::TokenKind::KW_MUT) {
        consume(Lex::TokenKind::KW_MUT);
        IsMut = true;
      } else if(currentToken().getKind() == Lex::TokenKind::KW_CONST) {
        consume(Lex::TokenKind::KW_CONST);
        IsMut = false;
      } else { 
        Diag.Report(DiagKind::Error,"Raw pointer types can only be mut or const.");
      }
      auto Pointee = parseType();
      if (!Pointee) return nullptr;
      End = currentToken().getLocation();
      Range = SourceRange(Start, End);
      return std::make_unique<PointerTypeName>(std::move(Pointee), IsMut, Range);
    }
    else if (currentToken().getKind() == Lex::TokenKind::OP_AMP) {
      consume(Lex::TokenKind::OP_AMP);
      bool IsMut;
      if(std::string(currentToken().getText()) == "mut") {
        consume(Lex::TokenKind::KW_MUT);
        IsMut = true;
      } else {
        IsMut = false;
      }
      auto Referent = parseType();
      if (!Referent) return nullptr;
      End = currentToken().getLocation();
      Range = SourceRange(Start, End);
      return std::make_unique<ReferenceTypeName>(std::move(Referent), IsMut, Range);
    } else if(currentToken().getKind() == Lex::TokenKind::DE_LBRACKET) {
      consume(Lex::TokenKind::DE_LBRACKET);
      auto Elemente = parseType();
      if (!Elemente) return nullptr;

      if(!expectToken(Lex::TokenKind::DE_SEMICOLON)) return nullptr;

      if(currentToken().getKind() != Lex::TokenKind::LT_INTEGER) {
        Diag.Report(DiagKind::Error, "Expected array size",
            currentToken().getLocation());
        return nullptr;
      }
      size_t Size = std::stoull(std::string(currentToken().getText()));
      consume(Lex::TokenKind::LT_INTEGER);
      if(!expectToken(Lex::TokenKind::DE_RBRACKET)) return nullptr;
      End = currentToken().getLocation();
      Range = SourceRange(Start, End);
      return std::make_unique<ArrayTypeName>(std::move(Elemente), Size, Range);
    } else {
      if (currentToken().getKind() != Lex::TokenKind::IDENTIFIER) {
        reportExpectedError(Lex::TokenKind::IDENTIFIER);
        return nullptr;
      }
      Lex::Token TypeNameToken = consume(Lex::TokenKind::IDENTIFIER);
      End = currentToken().getLocation();
      Range = SourceRange(Start, End);
      return std::make_unique<TypeName>(ASTNodeKind::ASTK_TYPENAME, 
          std::string(TypeNameToken.getText()), Range);
    }
  }

  std::unique_ptr<ExprStmt> Parser::parseExprStmt() {
    SourceLocation Start = currentToken().getLocation();
    auto Expression = parseExpr(0);
    if (!Expression) return nullptr;

    SourceLocation End = currentToken().getLocation();
    SourceRange Range = SourceRange(Start, End);
    if(!expectToken(Lex::TokenKind::DE_SEMICOLON)) return nullptr;

    return std::make_unique<ExprStmt>(Range, std::move(Expression));
  }

  std::unique_ptr<BlockStmt> Parser::parseBlockStmt() {
    SourceLocation Start = currentToken().getLocation();
    consume(Lex::TokenKind::DE_LBRACE);

    std::vector<std::unique_ptr<Stmt>> Statements;
    while(currentToken().getKind() != Lex::TokenKind::DE_RBRACE && !isAtEnd()) {
      auto S = parseStmt();
      if (S) {
        Statements.push_back(std::move(S));
      }
      else {
        while (currentToken().getKind() != Lex::TokenKind::DE_RBRACE && 
        currentToken().getKind() != Lex::TokenKind::DE_SEMICOLON && !isAtEnd()) {
          advance();
        }
       if (currentToken().getKind() == Lex::TokenKind::DE_SEMICOLON) advance();
      }
    }

    SourceLocation End = currentToken().getLocation();
    if(!expectToken(Lex::TokenKind::DE_RBRACE)) return nullptr;
    SourceRange Range = SourceRange(Start, End);
    return std::make_unique<BlockStmt>(Range, std::move(Statements));
  }

  std::unique_ptr<IfStmt> Parser::parseIfStmt() {
    SourceLocation Start = currentToken().getLocation();
    consume(Lex::TokenKind::KW_IF);

    std::unique_ptr<Expr> Condition = parseExpr(0);
    if (!Condition) return nullptr;

    std::unique_ptr<Stmt> Then = parseStmt();
    if(!Then) return nullptr;

    std::unique_ptr<Stmt> Else = nullptr;

    if(currentToken().getKind() == Lex::TokenKind::KW_ELSE) {
      consume(Lex::TokenKind::KW_ELSE);
      Else = parseStmt();
      if(!Else) return nullptr;
    }
    SourceLocation End = currentToken().getLocation();
    SourceRange Range = SourceRange(Start, End);

    return std::make_unique<IfStmt>(std::move(Condition), std::move(Then), 
        std::move(Else), Range);
  }

  std::unique_ptr<ForStmt> Parser::parseForStmt() {
    SourceLocation Start = currentToken().getLocation();
    consume(Lex::TokenKind::KW_FOR);

    auto InitRawPtr = parsePrimary().release();
    std::unique_ptr<VarExpr> Init(static_cast<VarExpr*>(InitRawPtr));
    if(!expectToken(Lex::TokenKind::KW_IN)) return nullptr;

    std::unique_ptr<RangeExpr> Range = parseRangeExpr(); 
    std::unique_ptr<Stmt> Body = parseStmt();

    SourceLocation End= currentToken().getLocation();
    SourceRange RangeLoc = SourceRange(Start, End);
    return std::make_unique<trsc::ForStmt>(std::move(Init), std::move(Range), 
        std::move(Body), RangeLoc);
  }

  std::unique_ptr<WhileStmt> Parser::parseWhileStmt() {
    SourceLocation Start = currentToken().getLocation();
    consume(Lex::TokenKind::KW_WHILE);
    std::unique_ptr<Expr> Condition = parseExpr(0);
    std::unique_ptr<Stmt> Block = parseStmt();
    SourceLocation End= currentToken().getLocation();
    SourceRange Range= SourceRange(Start, End);

    return std::make_unique<WhileStmt>(std::move(Condition), std::move(Block),
        Range);
  }

  std::unique_ptr<FuncDecl> Parser::parseFunction() {
    SourceLocation Start = currentToken().getLocation();
    consume(Lex::TokenKind::KW_FN);

    if (currentToken().getKind() != Lex::TokenKind::IDENTIFIER) {
      reportExpectedError(Lex::TokenKind::IDENTIFIER);
      return nullptr;
    }
    
    Lex::Token FuncNameToken = consume(Lex::TokenKind::IDENTIFIER);
    std::unique_ptr<VarExpr> FuncName = std::make_unique<VarExpr>(std::string(
          FuncNameToken.getText()));

    if(!expectToken(Lex::TokenKind::DE_LPAREN)) return nullptr;
  
    std::vector<FuncDecl::Param> ParamVector;
    if(currentToken().getKind() != Lex::TokenKind::DE_RPAREN) {
      while(true) {
        FuncDecl::Param MyParam;
        if (currentToken().getKind() != Lex::TokenKind::IDENTIFIER) {
          reportExpectedError(Lex::TokenKind::IDENTIFIER);
          return nullptr;
        }
        auto ParamNamePtr = parsePrimary().release();
        std::unique_ptr<VarExpr> ParamNameExpr(static_cast<VarExpr*>(ParamNamePtr));
        MyParam.ParamName = std::move(ParamNameExpr);
        if (currentToken().getKind() == Lex::TokenKind::DE_COLON) {
          consume(Lex::TokenKind::DE_COLON);
          MyParam.ParamType = parseType();
        }
        else MyParam.ParamType = nullptr;
        ParamVector.push_back(std::move(MyParam));

        if(currentToken().getKind() == Lex::TokenKind::DE_RPAREN) break;
        if(!expectToken(Lex::TokenKind::DE_COMMA)) return nullptr;
      }
    }

    consume(Lex::TokenKind::DE_RPAREN);
    std::unique_ptr<Type> FuncReturnType;
    if(currentToken().getKind() == Lex::TokenKind::DE_RETURNTYPE) {
      consume(Lex::TokenKind::DE_RETURNTYPE);
      FuncReturnType = parseType();
    }
    std::unique_ptr<BlockStmt> Block = parseBlockStmt();
    SourceLocation End = currentToken().getLocation();
    SourceRange LocRange = SourceRange(Start, End);

    return std::make_unique<FuncDecl>(LocRange, std::move(FuncName), 
        std::move(FuncReturnType), std::move(ParamVector), std::move(Block));
  }

  std::unique_ptr<FunCall> Parser::parseFunCall(std::optional<Lex::Token> 
      FuncNameToken = std::nullopt) {
    SourceLocation Start = currentToken().getLocation();
    if(!FuncNameToken) {
      if(currentToken().getKind() != Lex::TokenKind::IDENTIFIER) {
        reportExpectedError(Lex::TokenKind::IDENTIFIER);
      }
      consume(Lex::TokenKind::IDENTIFIER);
    }
    std::unique_ptr<VarExpr> FuncName = std::make_unique<VarExpr>(std::string(
          FuncNameToken->getText()));
    if(!expectToken(Lex::TokenKind::DE_LPAREN)) return nullptr;

    std::vector<std::unique_ptr<Expr>> ParamVector;
    if(currentToken().getKind() != Lex::TokenKind::DE_RPAREN) {
      while(true) {
        std::unique_ptr<Expr> Param = parsePrimary();
        ParamVector.emplace_back(std::move(Param));
        if(currentToken().getKind() == Lex::TokenKind::DE_RPAREN) break;
        if(!expectToken(Lex::TokenKind::DE_COMMA)) return nullptr;
      }
    }

    consume(Lex::TokenKind::DE_RPAREN);
    SourceLocation End = currentToken().getLocation();
    SourceRange Range = SourceRange(Start, End);

    return std::make_unique<FunCall>(Range, std::move(FuncName), std::move(ParamVector));
  }

  std::unique_ptr<ReturnStmt> Parser::parseReturnStmt() {
    SourceLocation Start = currentToken().getLocation();
    consume(Lex::TokenKind::KW_RETURN);

    std::unique_ptr<Expr> ReturnValue = nullptr;
    if (currentToken().getKind() != Lex::TokenKind::DE_SEMICOLON) {
      ReturnValue = parseExpr(0);
    }

    SourceLocation End = currentToken().getLocation();
    if (!expectToken(Lex::TokenKind::DE_SEMICOLON)) return nullptr;
    SourceRange Range = SourceRange(Start, End);

    return std::make_unique<ReturnStmt>(Range, std::move(ReturnValue));
  }

} // namespace trsc
