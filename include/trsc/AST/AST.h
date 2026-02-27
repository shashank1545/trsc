#ifndef TRSC_AST_AST_H
#define TRSC_AST_AST_H

#include "trsc/AST/QualType.h"
#include "trsc/Basic/SourceLocation.h"
#include "trsc/Lex/Token.h"
#include "trsc/AST/QualType.h"
#include "trsc/Sema/Scope.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace trsc {

enum class ASTNodeKind {
  ASTK_PROGRAM,
  ASTK_TYPE,
  ASTK_TYPENAME,
  ASTK_EXPR,
  ASTK_BOOLEXPR,
  ASTK_NUMEXPR,
  ASTK_INTEXPR,
  ASTK_FLOATEXPR,
  ASTK_VAREXPR,
  ASTK_BINEXPR,
  ASTK_RANGEEXPR,
  ASTK_STMT,
  ASTK_LETSTMT,
  ASTK_IFSTMT,
  ASTK_FORSTMT,
  ASTK_WHILESTMT,
  ASTK_EXPRSTMT,
  ASTK_BLOCKSTMT,
  ASTK_RETURNSTMT,
  ASTK_FUNCALL,
  ASTK_FUNCDECL,
};

const char* getASTKindName(ASTNodeKind Kind);

class ASTNode {
protected:
  SourceRange Loc;
  ASTNodeKind Kind;
  Scope* CurrentScope;

public:
  ASTNode(ASTNodeKind Kind, SourceRange Loc = {}) : Loc(Loc), Kind(Kind), 
  CurrentScope(nullptr) {}

  ASTNode(ASTNodeKind Kind, Scope* CurrentScope , SourceRange Loc = 
  {}) : Loc(Loc), Kind(Kind), CurrentScope(CurrentScope) {}

  virtual ~ASTNode() = default;
  SourceRange getSourceRange() const { return Loc; }
  ASTNodeKind getASTNodeKind() const { return Kind; }
  void setScope(Scope* MyScope)  { CurrentScope = MyScope; }
  Scope* getScope() const { return CurrentScope; }
};

class Type : public ASTNode {
protected:
  Type(ASTNodeKind Kind, SourceRange Loc = {}) : 
    ASTNode(Kind, Loc) {}

public:
  virtual std::string getName() const = 0;
};

class TypeName : public Type {
  std::string Name;
  
public:
  TypeName(ASTNodeKind Kind, const std::string &Name, SourceRange Loc = {})
      : Type(ASTNodeKind::ASTK_TYPENAME, Loc), Name(Name) {}
  std::string getName() const override { return Name; }
};

class Expr : public ASTNode {
private:
  QualType ExprType;

protected:
  Expr(ASTNodeKind Kind, SourceRange Loc = {})
      : ASTNode(Kind, Loc), ExprType() {}

public:
  QualType getType() const { return ExprType; }
  void setType(QualType T) { ExprType = T; }

  virtual bool isNum() const { return false; }
  virtual bool isVar() const { return false; }
  virtual bool isBool() const { return false; }
};

class Stmt : public ASTNode {
protected:
  Stmt(ASTNodeKind Kind, SourceRange Loc = {}) : ASTNode(Kind, Loc) {}
};

class NumExpr : public Expr {
  protected:
    NumExpr(ASTNodeKind Kind, SourceRange Loc = {})
      : Expr(Kind, Loc) {}

  public:
    bool isNum() const override { return true; }
    virtual bool isInt() const { return false; }
    virtual bool isFloat() const { return false; }
};

class IntExpr : public NumExpr {
  long long Value;
  public:
    IntExpr(long long Value, SourceRange Loc = {}): NumExpr(ASTNodeKind::ASTK_INTEXPR, Loc), 
    Value(Value) {}
    long long getValue() const {return Value;} 
    bool isInt() const override {return true;}
};

class FloatExpr: public NumExpr {
  double Value;
  public:
    FloatExpr(double Value, SourceRange Loc = {}): NumExpr(ASTNodeKind::ASTK_FLOATEXPR, Loc),
    Value(Value) {}
    double getValue() const {return Value;}
    bool isFloat() const override {return true;}
};

class BoolExpr : public Expr {
  bool Value;

public:
  BoolExpr(bool Value, SourceRange Loc = {})
      : Expr(ASTNodeKind::ASTK_BOOLEXPR, Loc), Value(Value) {}
  bool getValue() const { return Value; }
  bool isBool() const override { return true; }
};

class VarExpr : public Expr {
  std::string Name;

public:
  VarExpr(const std::string &Name, SourceRange Loc = {})
      : Expr(ASTNodeKind::ASTK_VAREXPR, Loc), Name(Name) {}
  const std::string &getName() const { return Name; }
  bool isVar() const override { return true; }
};

class BinExpr : public Expr {
  Lex::TokenKind Op;
  std::unique_ptr<Expr> LHS, RHS;

public:
  BinExpr(Lex::TokenKind Op, std::unique_ptr<Expr> LHS,
          std::unique_ptr<Expr> RHS, SourceRange Loc = {})
      : Expr(ASTNodeKind::ASTK_BINEXPR, Loc), Op(Op), LHS(std::move(LHS)),
        RHS(std::move(RHS)) {}
  Lex::TokenKind getOp() const { return Op; }
  Expr *getLHS() const { return LHS.get(); }
  Expr *getRHS() const { return RHS.get(); }
};

class RangeExpr : public Expr {
  bool IsInclusive;
  std::unique_ptr<Expr> Start, End;

public:
  RangeExpr(bool IsInclusive, std::unique_ptr<Expr> Start,
            std::unique_ptr<Expr> End, SourceRange Loc = {})
      : Expr(ASTNodeKind::ASTK_RANGEEXPR, Loc), IsInclusive(IsInclusive),
        Start(std::move(Start)), End(std::move(End)) {}

  bool isInclusive() const { return IsInclusive; }
  Expr *getStart() const { return Start.get(); }
  Expr *getEnd() const { return End.get(); }
};

class LetStmt : public Stmt {
  bool IsMut;
  std::unique_ptr<VarExpr> DeclaredVar;
  std::unique_ptr<TypeName> DeclaredType;
  std::unique_ptr<Expr> Initializer;

public:
  LetStmt(bool IsMut, std::unique_ptr<VarExpr> DeclaredVar,
          std::unique_ptr<TypeName> DeclaredType,
          std::unique_ptr<Expr> Initializer, SourceRange Loc = {})
      : Stmt(ASTNodeKind::ASTK_LETSTMT, Loc), IsMut(IsMut),
        DeclaredVar(std::move(DeclaredVar)),
        DeclaredType(std::move(DeclaredType)),
        Initializer(std::move(Initializer)) {}

  const bool isMut() const { return IsMut; }
  VarExpr *getDeclaredVar() const { return DeclaredVar.get(); }
  TypeName *getDeclaredType() const { return DeclaredType.get(); }
  Expr *getInitializer() const { return Initializer.get(); }
};

class BlockStmt : public Stmt {
  std::vector<std::unique_ptr<Stmt>> Statements;

public:
  BlockStmt(SourceRange Loc, std::vector<std::unique_ptr<Stmt>> Statements)
      : Stmt(ASTNodeKind::ASTK_BLOCKSTMT, Loc),
        Statements(std::move(Statements)) {}

  const std::vector<std::unique_ptr<Stmt>> &getStatements() const {
    return Statements;
  }
};

class IfStmt : public Stmt {
  std::unique_ptr<Expr> Condition;
  std::unique_ptr<Stmt> ThenBranch;
  std::unique_ptr<Stmt> ElseBranch;

public:
  IfStmt(std::unique_ptr<Expr> Condition, std::unique_ptr<Stmt> ThenBranch,
         std::unique_ptr<Stmt> ElseBranch, SourceRange Loc = {})
      : Stmt(ASTNodeKind::ASTK_IFSTMT, Loc), Condition(std::move(Condition)),
        ThenBranch(std::move(ThenBranch)), ElseBranch(std::move(ElseBranch)) {}

  Expr *getCondition() const { return Condition.get(); }
  Stmt *getThenBranch() const { return ThenBranch.get(); }
  Stmt *getElseBranch() const { return ElseBranch.get(); }
};

class ExprStmt : public Stmt {
  std::unique_ptr<Expr> Expression;

public:
  ExprStmt(SourceRange Loc, std::unique_ptr<Expr> Expression)
      : Stmt(ASTNodeKind::ASTK_EXPRSTMT, Loc),
        Expression(std::move(Expression)) {}

  Expr *getExpression() const { return Expression.get(); }
};

class ForStmt : public Stmt {
  std::unique_ptr<VarExpr> Init;
  std::unique_ptr<RangeExpr> Range;
  std::unique_ptr<Stmt> Body;

public:
  ForStmt(std::unique_ptr<VarExpr> Init, std::unique_ptr<RangeExpr> Range,
          std::unique_ptr<Stmt> Body, SourceRange Loc = {})
      : Stmt(ASTNodeKind::ASTK_FORSTMT, Loc), Init(std::move(Init)),
        Range(std::move(Range)), Body(std::move(Body)) {}

  VarExpr *getInit() const { return Init.get(); }
  RangeExpr *getRange() const { return Range.get(); }
  Stmt *getBody() const { return Body.get(); }
};

class WhileStmt : public Stmt {
  std::unique_ptr<Expr> Condition;
  std::unique_ptr<Stmt> Body;

public:
  WhileStmt(std::unique_ptr<Expr> Condition, std::unique_ptr<Stmt> Body,
            SourceRange Loc = {})
      : Stmt(ASTNodeKind::ASTK_WHILESTMT, Loc), Condition(std::move(Condition)),
        Body(std::move(Body)) {}

  Expr *getCondition() const { return Condition.get(); }
  Stmt *getBody() const { return Body.get(); }
};

class FuncDecl : public Stmt {
public:
  struct Param {
    std::unique_ptr<VarExpr> ParamName;
    std::unique_ptr<TypeName> ParamType;
  };

private:
  std::unique_ptr<VarExpr> FuncName;
  std::unique_ptr<TypeName> FuncReturnType;
  std::vector<Param> Params;
  std::unique_ptr<Stmt> Body;

public:
  FuncDecl(SourceRange Loc, std::unique_ptr<VarExpr> Name,
           std::unique_ptr<TypeName> FuncReturnType, std::vector<Param> Params,
           std::unique_ptr<Stmt> Body)
      : Stmt(ASTNodeKind::ASTK_FUNCDECL, Loc), FuncName(std::move(Name)),
        FuncReturnType(std::move(FuncReturnType)), Params(std::move(Params)),
        Body(std::move(Body)) {}

  const std::vector<Param> &getParams() const { return Params; }
  VarExpr* getFuncName() const { return FuncName.get(); }
  TypeName* getReturnType() const { return FuncReturnType.get(); }
  Stmt *getBody() const { return Body.get(); }
};

class FunCall: public Expr {
private:
  std::unique_ptr<VarExpr> FuncName;
  std::vector<std::unique_ptr<Expr>> Params;

public:
  FunCall(SourceRange Range, std::unique_ptr<VarExpr> Name,
           std::vector<std::unique_ptr<Expr>> Params)
      : Expr(ASTNodeKind::ASTK_FUNCALL, Range), FuncName(std::move(Name)),
        Params(std::move(Params)) {}

  VarExpr* getFuncName() const { return FuncName.get(); }
  const std::vector<std::unique_ptr<Expr>> &getParams() const { return Params;}
};

class ReturnStmt : public Stmt {
  std::unique_ptr<Expr> ReturnValue;

public:
  ReturnStmt(SourceRange Loc, std::unique_ptr<Expr> ReturnValue)
      : Stmt(ASTNodeKind::ASTK_RETURNSTMT, Loc),
        ReturnValue(std::move(ReturnValue)) {}

  Expr *getReturnValue() const { return ReturnValue.get(); }
};

class Program : public ASTNode {
  std::vector<std::unique_ptr<Stmt>> Statements;

public:
  Program(SourceRange Loc, std::vector<std::unique_ptr<Stmt>> Statements)
      : ASTNode(ASTNodeKind::ASTK_PROGRAM, Loc),
        Statements(std::move(Statements)) {}
  const std::vector<std::unique_ptr<Stmt>> &getStatements() const {
    return Statements;
  }
};

} // namespace trsc

#endif // TRSC_AST_AST_H
