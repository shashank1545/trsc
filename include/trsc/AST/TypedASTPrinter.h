
#ifndef TRSC_AST_TYPEDASTPRINTER_H
#define TRSC_AST_TYPEDASTPRINTER_H

#include "trsc/AST/AST.h"
#include "trsc/AST/ASTVisitor.h"

#include <iostream>
#include <ostream>

namespace trsc {
  class TypedASTPrinter : public ASTVisitor<TypedASTPrinter> {
    private:
      std::ostream &OS;
      unsigned IndentLevel;
      std::vector<bool> IsLastStack;

      void printIndent(bool isLast);
      void printNodeHeader(const ASTNode *Node, const std::string &NodeName);
      std::string getLocationString(const ASTNode *Node);
      std::string getTypeString(const Expr *E);
      std::string getAddressString(const void *Ptr);

    public:
      TypedASTPrinter(std::ostream &OS);

      // Make the base class visit methods visible
      using ASTVisitor<TypedASTPrinter>::visit;

      void visitProgram(Program *Node);

      // Visitors for concrete statement nodes
      void visitLetStmt(LetStmt *Node);
      void visitIfStmt(IfStmt *Node);
      void visitWhileStmt(WhileStmt *Node);
      void visitForStmt(ForStmt *Node);
      void visitBlockStmt(BlockStmt *Node);
      void visitExprStmt(ExprStmt *Node);
      void visitReturnStmt(ReturnStmt *Node);
      void visitFuncDecl(FuncDecl *Node);
      
      // Visitors for concrete expression nodes
      void visitIntExpr(IntExpr *Node);
      void visitFloatExpr(FloatExpr *Node);
      void visitVarExpr(VarExpr *Node);
      void visitRefrExpr(RefrExpr *Node);
      void visitBoolExpr(BoolExpr *Node);
      void visitBinExpr(BinExpr *Node);
      void visitASExpr(ASExpr *Node);
      void visitRangeExpr(RangeExpr *Node);
      void visitFunCall(FunCall * Node);

      // Visitors for concrete type nodes
      void visitTypeName(TypeName *Node);
      void visitPointerTypeName(PointerTypeName *Node);
      void visitReferenceTypeName(ReferenceTypeName *Node);
      void visitArrayTypeName(ArrayTypeName *Node);
  };

} //namespace  trsc 
  
#endif // TRSC_AST_TYPEDASTPRINTER_H
