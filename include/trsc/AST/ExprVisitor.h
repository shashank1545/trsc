#ifndef TRSC_AST_EXPRVISITOR_H
#define TRSC_AST_EXPRVISITOR_H

#include "trsc/AST/AST.h"

namespace trsc {

/// ExprVisitor - CRTP-based visitor for expression nodes that returns a value.
/// This is useful for code generation where visiting an expression produces
/// an output value (e.g., mlir::Value, llvm::Value, etc.)
///
/// Template parameters:
///   Derived - The derived visitor class (CRTP)
///   RetTy   - The return type for visit methods (e.g., mlir::Value)
///
/// Example usage:
///   class MLIRExprGen : public ExprVisitor<MLIRExprGen, mlir::Value> {
///     mlir::Value visitIntExpr(IntExpr *E) {
///       return builder.create<ConstantOp>(loc, E->getValue());
///     }
///   };
///
template <typename Derived, typename RetTy>
class ExprVisitor {
public:
  /// Visit an expression node and return the generated value.
  /// This is the main entry point for the visitor.
  RetTy visit(Expr *E) {
    if (!E)
      return RetTy();  // Return default-constructed value for null

    switch(E->getASTNodeKind()) {
      case ASTNodeKind::ASTK_BOOLEXPR:
        return getDerived().visitBoolExpr(static_cast<BoolExpr*>(E));
      case ASTNodeKind::ASTK_VAREXPR:
        return getDerived().visitVarExpr(static_cast<VarExpr*>(E));
      case ASTNodeKind::ASTK_ASEXPR:
        return getDerived().visitASExpr(static_cast<ASExpr*>(E));
      case ASTNodeKind::ASTK_INTEXPR:
        return getDerived().visitIntExpr(static_cast<IntExpr*>(E));
      case ASTNodeKind::ASTK_FLOATEXPR:
        return getDerived().visitFloatExpr(static_cast<FloatExpr*>(E));
      case ASTNodeKind::ASTK_BINEXPR:
        return getDerived().visitBinExpr(static_cast<BinExpr*>(E));
      case ASTNodeKind::ASTK_REFREXPR:
        return getDerived().visitRefrExpr(static_cast<RefrExpr*>(E));
      case ASTNodeKind::ASTK_RANGEEXPR:
        return getDerived().visitRangeExpr(static_cast<RangeExpr*>(E));
      case ASTNodeKind::ASTK_FUNCALL:
        return getDerived().visitFunCall(static_cast<FunCall*>(E));
      default:
        // For base expression types or unknown kinds, return default value
        return RetTy();
    }
  }

  // Default implementations for leaf expressions
  // Derived classes should override these to provide actual code generation
  RetTy visitBoolExpr(BoolExpr *E) { return RetTy(); }
  RetTy visitVarExpr(VarExpr *E) { return RetTy(); }
  RetTy visitIntExpr(IntExpr *E) { return RetTy(); }
  RetTy visitFloatExpr(FloatExpr *E) { return RetTy(); }
  
  // Default implementations for composite expressions
  // These visit children and return a default value
  RetTy visitBinExpr(BinExpr *E) {
    getDerived().visit(E->getLHS());
    getDerived().visit(E->getRHS());
    return RetTy();
  }

  RetTy visitRefrExpr(RefrExpr *E) {
    getDerived().visit(E->getReferent());
    return RetTy();
  }

  RetTy visitASExpr(ASExpr *E) {
    getDerived().visit(E->getFromExpr());
    getDerived().visit(E->getToType());
    return RetTy();
  }
  
  RetTy visitRangeExpr(RangeExpr *E) {
    getDerived().visit(E->getStart());
    getDerived().visit(E->getEnd());
    return RetTy();
  }
  
  RetTy visitFunCall(FunCall *E) {
    for (const auto &Arg : E->getParams()) {
      getDerived().visit(Arg.get());
    }
    return RetTy();
  }

protected:
  Derived &getDerived() { return *static_cast<Derived *>(this); }
};

} // namespace trsc

#endif // TRSC_AST_EXPRVISITOR_H
