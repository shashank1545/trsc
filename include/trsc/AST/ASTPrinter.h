#ifndef TRSC_AST_ASTPRINTER_H
#define TRSC_AST_ASTPRINTER_H

#include "trsc/AST/AST.h"
#include "trsc/AST/ASTVisitor.h"

#include <iostream>
#include <ostream>

namespace trsc {
  class ASTPrinter : public ASTVisitor<ASTPrinter> {
    private:
      std::ostream &OS;
      unsigned IndentLevel = 0;
      void indent();

    public:
      ASTPrinter(std::ostream &OS) : OS(OS) {}

      // Make the base class visit methods visible
      using ASTVisitor<ASTPrinter>::visit;

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
      void visitBoolExpr(BoolExpr *Node);
      void visitBinExpr(BinExpr *Node);
      void visitRangeExpr(RangeExpr *Node);
      void visitFunCall(FunCall * Node);

      // Visitors for concrete type nodes
      void visitTypeName(TypeName *Node);
  };

} //namespace  trsc 
  
#endif // TRSC_AST_ASTPRINTER_H 
