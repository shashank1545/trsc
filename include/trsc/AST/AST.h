#ifndef TRSC_AST_AST_H
#define TRSC_AST_AST_H

#include "trsc/AST/ASTContext.h"
#include "trsc/AST/QualType.h"
#include "trsc/Basic/ArrayRef.h"
#include "trsc/Basic/IdentifierTable.h"
#include "trsc/Lex/Token.h"

#include <cstdint>
#include <vector>

namespace trsc {

class Scope;
struct Symbol;

enum class ASTNodeKind {
  ASTK_PROGRAM,
  ASTK_TYPE,
  ASTK_TYPENAME,
  ASTK_REFERTYPENAME,
  ASTK_POINTERTYPENAME,
  ASTK_ARRAYTYPENAME,
  ASTK_EXPR,
  ASTK_ASEXPR,
  ASTK_BOOLEXPR,
  ASTK_NUMEXPR,
  ASTK_INTEXPR,
  ASTK_FLOATEXPR,
  ASTK_VAREXPR,
  ASTK_REFREXPR,
  ASTK_ARRAYEXPR,
  ASTK_ARRAYACCESSEXPR,
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

/// Every AST node lives in the ASTContext arena: allocated with
/// `new (Ctx) Node(...)`, never destroyed individually, freed wholesale when
/// the context dies. Nodes therefore hold raw non-owning pointers to their
/// children and must stay trivially destructible (enforced by static_asserts
/// in lib/AST/AST.cpp).
class ASTNode {
protected:
  SourceRange Loc;
  ASTNodeKind Kind;
  Scope *CurrentScope;

public:
  ASTNode(ASTNodeKind Kind, SourceRange Loc = {})
      : Loc(Loc), Kind(Kind), CurrentScope(nullptr) {}

  void *operator new(size_t Bytes, const ASTContext &Ctx, size_t Align = 8) {
    return Ctx.Allocate(Bytes, Align);
  }

  void operator delete(void *, const ASTContext &, size_t) {}

  void *operator new(size_t) = delete;
  void operator delete(void *) = delete;

  SourceRange getSourceRange() const { return Loc; }
  ASTNodeKind getASTNodeKind() const { return Kind; }
  void setScope(Scope *MyScope) { CurrentScope = MyScope; }
  Scope *getScope() const { return CurrentScope; }
  virtual bool isStmt() const { return false; }
  virtual bool isExpr() const { return false; }
};

class Type : public ASTNode {
protected:
  Type(ASTNodeKind Kind, SourceRange Loc = {}) : ASTNode(Kind, Loc) {}

public:
  virtual std::string getName() const = 0;
  virtual bool isMut() const { return false; }
};

class TypeName : public Type {
  const IdentifierInfo *Name;

public:
  TypeName(const IdentifierInfo *Name, SourceRange Loc = {})
      : Type(ASTNodeKind::ASTK_TYPENAME, Loc), Name(Name) {}
  std::string getName() const override { return Name->getName(); }
};

class PointerTypeName : public Type {
  Type *Pointee;
  bool IsMut;

public:
  PointerTypeName(Type *Pointee, bool IsMut, SourceRange Loc = {})
      : Type(ASTNodeKind::ASTK_POINTERTYPENAME, Loc), Pointee(Pointee),
        IsMut(IsMut) {}

  Type *getPointee() const { return Pointee; }
  bool isMut() const override { return IsMut; }
  std::string getName() const override {
    return (IsMut ? "*mut " : "*const ") + Pointee->getName();
  }
};

class ReferenceTypeName : public Type {
  Type *Referent;
  bool IsMut;

public:
  ReferenceTypeName(Type *Referent, bool IsMut, SourceRange Loc = {})
      : Type(ASTNodeKind::ASTK_REFERTYPENAME, Loc), Referent(Referent),
        IsMut(IsMut) {}

  Type *getReferent() const { return Referent; }
  bool isMut() const override { return IsMut; }
  std::string getName() const override {
    return (IsMut ? "&mut " : "&") + Referent->getName();
  }
};

class ArrayTypeName : public Type {
  Type *Elemente;
  size_t Size;

public:
  ArrayTypeName(Type *Elemente, size_t Size, SourceRange Loc = {})
      : Type(ASTNodeKind::ASTK_ARRAYTYPENAME, Loc), Elemente(Elemente),
        Size(Size) {}

  Type *getElemente() const { return Elemente; }
  size_t getSize() const { return Size; }
  std::string getName() const override {
    return "[" + Elemente->getName() + "; " + std::to_string(Size) + "]";
  }
};

class Expr : public ASTNode {
private:
  QualType ExprType;

protected:
  Expr(ASTNodeKind Kind, SourceRange Loc = {})
      : ASTNode(Kind, Loc), ExprType() {}

public:
  QualType &getType() { return ExprType; }
  void setType(QualType T) { ExprType = T; }

  virtual bool isNum() const { return false; }
  virtual bool isVar() const { return false; }
  virtual bool isBool() const { return false; }

  bool isExpr() const override { return true; }
};

class Stmt : public ASTNode {
protected:
  Stmt(ASTNodeKind Kind, SourceRange Loc = {}) : ASTNode(Kind, Loc) {}
  bool isStmt() const override { return true; }
};

class NumExpr : public Expr {
protected:
  NumExpr(ASTNodeKind Kind, SourceRange Loc = {}) : Expr(Kind, Loc) {}

public:
  bool isNum() const override { return true; }
  virtual bool isInt() const { return false; }
  virtual bool isFloat() const { return false; }
  static bool classof(const Expr *E) {
    return E->getASTNodeKind() == ASTNodeKind::ASTK_NUMEXPR;
  }
};

class IntExpr : public NumExpr {
  int64_t Value;

public:
  IntExpr(int64_t Value, SourceRange Loc = {})
      : NumExpr(ASTNodeKind::ASTK_INTEXPR, Loc), Value(Value) {}
  int64_t getValue() const { return Value; }
  bool isInt() const override { return true; }
  static bool classof(const Expr *E) {
    return E->getASTNodeKind() == ASTNodeKind::ASTK_INTEXPR;
  }
};

class FloatExpr : public NumExpr {
  double Value;

public:
  FloatExpr(double Value, SourceRange Loc = {})
      : NumExpr(ASTNodeKind::ASTK_FLOATEXPR, Loc), Value(Value) {}
  double getValue() const { return Value; }
  bool isFloat() const override { return true; }
  static bool classof(const Expr *E) {
    return E->getASTNodeKind() == ASTNodeKind::ASTK_FLOATEXPR;
  }
};

class BoolExpr : public Expr {
  bool Value;

public:
  BoolExpr(bool Value, SourceRange Loc = {})
      : Expr(ASTNodeKind::ASTK_BOOLEXPR, Loc), Value(Value) {}
  bool getValue() const { return Value; }
  bool isBool() const override { return true; }
  static bool classof(const Expr *E) {
    return E->getASTNodeKind() == ASTNodeKind::ASTK_BOOLEXPR;
  }
};

class VarExpr : public Expr {
  const IdentifierInfo *Id;
  Symbol *ResolvedSym = nullptr;

public:
  VarExpr(const IdentifierInfo *Id, SourceRange Loc = {})
      : Expr(ASTNodeKind::ASTK_VAREXPR, Loc), Id(Id) {}
  const std::string &getName() const { return Id->getName(); }
  const IdentifierInfo *getIdentifierInfo() const { return Id; }
  Symbol *getSymbol() const { return ResolvedSym; }
  void setSymbol(Symbol *S) { ResolvedSym = S; }
  bool isVar() const override { return true; }
  static bool classof(const Expr *E) {
    return E->getASTNodeKind() == ASTNodeKind::ASTK_VAREXPR;
  }
};

class RefrExpr : public Expr {
  Expr *ReferentExpr;
  bool IsMut;

public:
  RefrExpr(Expr *ReferentExpr, bool IsMut, SourceRange Loc = {})
      : Expr(ASTNodeKind::ASTK_REFREXPR, Loc), ReferentExpr(ReferentExpr),
        IsMut(IsMut) {}
  Expr *getReferent() const { return ReferentExpr; }
  bool isMut() const { return IsMut; }
  static bool classof(const Expr *E) {
    return E->getASTNodeKind() == ASTNodeKind::ASTK_REFREXPR;
  }
};

class BinExpr : public Expr {
  Lex::TokenKind Op;
  Expr *LHS, *RHS;

public:
  BinExpr(Lex::TokenKind Op, Expr *LHS, Expr *RHS, SourceRange Loc = {})
      : Expr(ASTNodeKind::ASTK_BINEXPR, Loc), Op(Op), LHS(LHS), RHS(RHS) {}
  Lex::TokenKind getOp() const { return Op; }
  Expr *getLHS() const { return LHS; }
  Expr *getRHS() const { return RHS; }
  static bool classof(const Expr *E) {
    return E->getASTNodeKind() == ASTNodeKind::ASTK_BINEXPR;
  }
};

class ASExpr : public Expr {
private:
  Expr *FromExpr;
  Type *ToType;

public:
  ASExpr(Expr *FromExpr, Type *ToType, SourceRange Loc = {})
      : Expr(ASTNodeKind::ASTK_ASEXPR, Loc), FromExpr(FromExpr),
        ToType(ToType) {}

  Expr *getFromExpr() const { return FromExpr; }
  Type *getToType() const { return ToType; }
  static bool classof(const Expr *E) {
    return E->getASTNodeKind() == ASTNodeKind::ASTK_ASEXPR;
  }
};

class ArrayExpr : public Expr {
private:
  ArrayRef<Expr *> ChildElemExprVec;
  /* For n dimensional array , LastDim will give the number/count of the
     underlying n-1 dimensional array.*/
  IntExpr *LastDim;
  ArrayRef<int64_t> Shape;

public:
  ArrayExpr(ASTContext &C, const std::vector<Expr *> &ChildElemExprVec,
            IntExpr *LastDim, const std::vector<int64_t> &Shape,
            SourceRange Loc = {})
      : Expr(ASTNodeKind::ASTK_ARRAYEXPR, Loc),
        ChildElemExprVec(C.allocateArray(ChildElemExprVec)), LastDim(LastDim),
        Shape(C.allocateArray(Shape)) {}

  ArrayRef<Expr *> getChildElemExprVec() const { return ChildElemExprVec; }
  IntExpr *getTrailingDim() const { return LastDim; }
  ArrayRef<int64_t> getShape() const { return Shape; }
  static bool classof(const Expr *E) {
    return E->getASTNodeKind() == ASTNodeKind::ASTK_ARRAYEXPR;
  }
};

class ArrayAccessExpr : public Expr {
private:
  VarExpr *ArrayNameExpr;
  ArrayRef<Expr *> IndexExprVec;

public:
  ArrayAccessExpr(ASTContext &C, VarExpr *ArrayNameExpr,
                  const std::vector<Expr *> &IndexExprVec, SourceRange Loc = {})
      : Expr(ASTNodeKind::ASTK_ARRAYACCESSEXPR, Loc),
        ArrayNameExpr(ArrayNameExpr),
        IndexExprVec(C.allocateArray(IndexExprVec)) {}

  VarExpr *getArrayNameExpr() { return ArrayNameExpr; }
  ArrayRef<Expr *> getIndexVector() const { return IndexExprVec; }
  static bool classof(const Expr *E) {
    return E->getASTNodeKind() == ASTNodeKind::ASTK_ARRAYACCESSEXPR;
  }
};

class RangeExpr : public Expr {
  bool IsInclusive;
  Expr *Start, *End;

public:
  RangeExpr(bool IsInclusive, Expr *Start, Expr *End, SourceRange Loc = {})
      : Expr(ASTNodeKind::ASTK_RANGEEXPR, Loc), IsInclusive(IsInclusive),
        Start(Start), End(End) {}

  bool isInclusive() const { return IsInclusive; }
  Expr *getStart() const { return Start; }
  Expr *getEnd() const { return End; }
  static bool classof(const Expr *E) {
    return E->getASTNodeKind() == ASTNodeKind::ASTK_RANGEEXPR;
  }
};

class LetStmt : public Stmt {
  bool IsMut;
  VarExpr *DeclaredVar;
  Type *DeclaredType;
  Expr *Initializer;

public:
  LetStmt(bool IsMut, VarExpr *DeclaredVar, Type *DeclaredType,
          Expr *Initializer, SourceRange Loc = {})
      : Stmt(ASTNodeKind::ASTK_LETSTMT, Loc), IsMut(IsMut),
        DeclaredVar(DeclaredVar), DeclaredType(DeclaredType),
        Initializer(Initializer) {}

  bool isMut() const { return IsMut; }
  VarExpr *getDeclaredVar() const { return DeclaredVar; }
  Type *getDeclaredType() const { return DeclaredType; }
  Expr *getInitializer() const { return Initializer; }
};

class BlockStmt : public Stmt {
  ArrayRef<Stmt *> Statements;

public:
  BlockStmt(ASTContext &C, SourceRange Loc,
            const std::vector<Stmt *> &Statements)
      : Stmt(ASTNodeKind::ASTK_BLOCKSTMT, Loc),
        Statements(C.allocateArray(Statements)) {}

  ArrayRef<Stmt *> getStatements() const { return Statements; }
};

class IfStmt : public Stmt {
  Expr *Condition;
  Stmt *ThenBranch;
  Stmt *ElseBranch;

public:
  IfStmt(Expr *Condition, Stmt *ThenBranch, Stmt *ElseBranch,
         SourceRange Loc = {})
      : Stmt(ASTNodeKind::ASTK_IFSTMT, Loc), Condition(Condition),
        ThenBranch(ThenBranch), ElseBranch(ElseBranch) {}

  Expr *getCondition() const { return Condition; }
  Stmt *getThenBranch() const { return ThenBranch; }
  Stmt *getElseBranch() const { return ElseBranch; }
};

class ExprStmt : public Stmt {
  Expr *Expression;

public:
  ExprStmt(SourceRange Loc, Expr *Expression)
      : Stmt(ASTNodeKind::ASTK_EXPRSTMT, Loc), Expression(Expression) {}

  Expr *getExpression() const { return Expression; }
};

class ForStmt : public Stmt {
  VarExpr *Init;
  RangeExpr *Range;
  Stmt *Body;

public:
  ForStmt(VarExpr *Init, RangeExpr *Range, Stmt *Body, SourceRange Loc = {})
      : Stmt(ASTNodeKind::ASTK_FORSTMT, Loc), Init(Init), Range(Range),
        Body(Body) {}

  VarExpr *getInit() const { return Init; }
  RangeExpr *getRange() const { return Range; }
  Stmt *getBody() const { return Body; }
};

class WhileStmt : public Stmt {
  Expr *Condition;
  Stmt *Body;

public:
  WhileStmt(Expr *Condition, Stmt *Body, SourceRange Loc = {})
      : Stmt(ASTNodeKind::ASTK_WHILESTMT, Loc), Condition(Condition),
        Body(Body) {}

  Expr *getCondition() const { return Condition; }
  Stmt *getBody() const { return Body; }
};

class FuncDecl : public Stmt {
public:
  // Trivially copyable; the param list is one arena array.
  struct Param {
    VarExpr *ParamName = nullptr;
    Type *ParamType = nullptr;
  };

private:
  VarExpr *FuncName;
  Type *FuncReturnType;
  ArrayRef<Param> Params;
  Stmt *Body;

public:
  FuncDecl(ASTContext &C, SourceRange Loc, VarExpr *Name, Type *FuncReturnType,
           const std::vector<Param> &Params, Stmt *Body)
      : Stmt(ASTNodeKind::ASTK_FUNCDECL, Loc), FuncName(Name),
        FuncReturnType(FuncReturnType), Params(C.allocateArray(Params)),
        Body(Body) {}

  ArrayRef<Param> getParams() const { return Params; }
  VarExpr *getFuncName() const { return FuncName; }
  Type *getReturnType() const { return FuncReturnType; }
  Stmt *getBody() const { return Body; }
};

class FunCall : public Expr {
private:
  VarExpr *FuncName;
  ArrayRef<Expr *> Params;

public:
  FunCall(ASTContext &C, SourceRange Range, VarExpr *Name,
          const std::vector<Expr *> &Params)
      : Expr(ASTNodeKind::ASTK_FUNCALL, Range), FuncName(Name),
        Params(C.allocateArray(Params)) {}

  VarExpr *getFuncName() const { return FuncName; }
  ArrayRef<Expr *> getParams() const { return Params; }
  static bool classof(const Expr *E) {
    return E->getASTNodeKind() == ASTNodeKind::ASTK_FUNCALL;
  }
};

class ReturnStmt : public Stmt {
  Expr *ReturnValue;

public:
  ReturnStmt(SourceRange Loc, Expr *ReturnValue)
      : Stmt(ASTNodeKind::ASTK_RETURNSTMT, Loc), ReturnValue(ReturnValue) {}

  Expr *getReturnValue() const { return ReturnValue; }
};

class Program : public ASTNode {
  ArrayRef<Stmt *> Statements;

public:
  Program(ASTContext &C, SourceRange Loc, const std::vector<Stmt *> &Statements)
      : ASTNode(ASTNodeKind::ASTK_PROGRAM, Loc),
        Statements(C.allocateArray(Statements)) {}
  ArrayRef<Stmt *> getStatements() const { return Statements; }
};

} // namespace trsc

#endif // TRSC_AST_AST_H
