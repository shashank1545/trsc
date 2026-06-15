#include "trsc/Sema/TypeChecker.h"
#include "trsc/AST/ASTContext.h"
#include "trsc/Basic/Diagnostics.h"
#include "trsc/Sema/SymbolTable.h"

namespace trsc {

  TypeChecker::TypeChecker(DiagnosticsEngine &Diags, SymbolTable &ST, 
      ASTContext &Ctx): Diags(Diags), ST(ST), Ctx(Ctx) {}

  QualType TypeChecker::resolveType(Type *T) {
    switch(T->getASTNodeKind()) {
      case ASTNodeKind::ASTK_TYPENAME: {
          auto *TN = static_cast<TypeName*>(T);
          return Ctx.getTypeByName(TN->getName());
        }
      case ASTNodeKind::ASTK_POINTERTYPENAME: {
        auto *PT = static_cast<PointerTypeName*>(T);
        QualType Pointee = resolveType(PT->getPointee());
        return Ctx.getPointerType(Pointee, PT->isMut());
      }
      case ASTNodeKind::ASTK_REFERTYPENAME: {
        auto *RT = static_cast<ReferenceTypeName*>(T);
        QualType Referent = resolveType(RT->getReferent());
        return Ctx.getReferenceType(Referent, RT->isMut());
      }
      case ASTNodeKind::ASTK_ARRAYTYPENAME: {
        auto *AT = static_cast<ArrayTypeName*>(T);
        QualType Element = resolveType(AT->getElemente());
        return Ctx.getArrayType(Element, AT->getSize());
      }
      default: {
        Diags.Report(DiagKind::Error, "Unsupported node used as type.");
        return Ctx.getNullType();
      }
    }
  }

  void TypeChecker::visitASExpr(ASExpr *Node) {
    visit(Node->getFromExpr());
    QualType FromType = Node->getFromExpr()->getType();
    QualType ToType = resolveType(Node->getToType());
    bool Convertable = Ctx.canExplicitlyConvert(FromType, ToType);   
    if(Convertable) {
      Node->setType(ToType);
    }
    else {
      std::string ErrMsg = "Cannot convert " + FromType.getAsString() + " to " 
        + ToType.getAsString() + " \n";
      Diags.Report(DiagKind::Error, ErrMsg); 
    }
  }

  void TypeChecker::visitIntExpr(IntExpr *Node) {
    if(!ExpectedType.isNull() && ExpectedType.isIntegerType() &&
        (!ExpectedType.isReferenceType() && !ExpectedType.isPointerType())) {
      Node->setType(ExpectedType);
    } else Node->setType(Ctx.getI32Type());
  }

  void TypeChecker::visitFloatExpr(FloatExpr *Node) {
    if(!ExpectedType.isNull() && ExpectedType.isFloatingType() &&  
        (!ExpectedType.isReferenceType() && !ExpectedType.isPointerType())) {
      Node->setType(ExpectedType);
    } else Node->setType(Ctx.getF64Type());
  }

  void TypeChecker::visitRefrExpr(RefrExpr *Node) {
    visit(Node->getReferent());
    QualType ReferentType = Node->getReferent()->getType();
    Node->setType(Ctx.getReferenceType(ReferentType, Node->isMut()));
  }

  void TypeChecker::visitVarExpr(VarExpr *Node) {
    auto Sym = ST.lookupSymbol(Node->getName(), Node->getScope());
    if (Sym) {
      Node->setType(Sym->Ty);
    }
  }

  void TypeChecker::visitBoolExpr(BoolExpr *Node) {
    Node->setType(Ctx.getBoolType());
  }

  void TypeChecker::visitArrayExpr(ArrayExpr *Node) {
    if (Node->getTrailingDim()->getValue() < 0) {
      Diags.Report(DiagKind::Error, "Array size cannot be negative.");
      return;
    }

    QualType OldExpectedType = ExpectedType;
    ExpectedType = OldExpectedType.isArrayType()
      ? OldExpectedType.getBaseType()
      : QualType();

    for (const auto &Child : Node->getChildElemExprVec()) {
      visit(Child.get());
    }

    ExpectedType = OldExpectedType;

    if (!OldExpectedType.isNull() && OldExpectedType.isArrayType() &&
        !OldExpectedType.isReferenceType() && !OldExpectedType.isPointerType()) {
      Node->setType(OldExpectedType);
    } else {
      Node->setType(Ctx.getArrayType(
            Node->getChildElemExprVec()[0]->getType(),
            static_cast<size_t>(Node->getTrailingDim()->getValue())));
    }
  }

  // void TypeChecker::visitArrayAccessExpr(ArrayAccessExpr *Node) {
  //   Symbol* Sym = ST.lookupSymbol(Node->getArrayNameExpr()->getName(), 
  //       Node->getScope());
  //   if(!Sym) Diags.Report(DiagKind::Error, "Array not found in SymbolTable.");
  //   if(!Sym->Ty.isArrayType()) {
  //     Diags.Report(DiagKind::Error, Node->getArrayNameExpr()->getName() + 
  //         " is not an array.");
  //   }
  //   const ArrayType* ArrayTyPtr = dynamic_cast<const ArrayType*>(
  //       Sym->Ty.getTypePtr());
  //   bool IsElementArray = true;
  //   for(int I = 0; I < Node->getIndexVector().size(); ++I) {
  //     if(!IsElementArray) {
  //       Diags.Report(DiagKind::Error, "Unable to access " + std::to_string(I) + 
  //           " dimension " + "in an array with" + std::to_string(I-1) + 
  //           " dimension.");
  //       break;
  //     }
  //     size_t ArraySize = ArrayTyPtr->getArraySize();
  //     if(ArrayTyPtr->getElementType().isArrayType()) {
  //       ArrayTyPtr = dynamic_cast<const ArrayType*>(ArrayTyPtr->
  //           getElementType().getTypePtr());
  //     } else IsElementArray = false;
  //     if(Node->getIndexVector()[I]->isNum()) {
  //       NumExpr* Num = static_cast<NumExpr*>(Node->getIndexVector()[I].get());
  //       if(Num->isInt()) {
  //         int64_t IndexValue = static_cast<IntExpr*>(Num)->getValue();
  //         if(IndexValue < 0) { 
  //           Diags.Report(DiagKind::Error, "Array index cannot be negative.");
  //         }
  //         if(IndexValue > ArraySize) {
  //           Diags.Report(DiagKind::Error, "Array index out of bound.");
  //         }
  //       } else Diags.Report(DiagKind::Error, "Index can only be integers.");
  //     }
  //   }
  // }

  void TypeChecker::visitArrayAccessExpr(ArrayAccessExpr *Node) {
    Symbol *Sym = ST.lookupSymbol(Node->getArrayNameExpr()->getName(),
        Node->getArrayNameExpr()->getScope());
    if (!Sym) {
      Diags.Report(DiagKind::Error, "Array not found in SymbolTable.",
          Node->getSourceRange().getStart());
      return;
    }
    if (!Sym->Ty.isArrayType()) {
      Diags.Report(DiagKind::Error, 
          Node->getArrayNameExpr()->getName() + " is not an array.",
          Node->getSourceRange().getStart());
      return;
    }

    const ArrayType *ArrayTyPtr = static_cast<const ArrayType*>(
        Sym->Ty.getTypePtr());

    for (size_t I = 0; I < Node->getIndexVector().size(); ++I) {
      if (!ArrayTyPtr) {
        Diags.Report(DiagKind::Error,
            "Too many indices: array has fewer dimensions.",
            Node->getIndexVector()[I]->getSourceRange().getStart());
        return;
      }

      visit(Node->getIndexVector()[I].get());

      QualType IdxTy = Node->getIndexVector()[I]->getType();
      if (!IdxTy.isIntegerType()) {
        Diags.Report(DiagKind::Error, "Array index must be an integer type.",
            Node->getIndexVector()[I]->getSourceRange().getStart());
      }

      if (Node->getIndexVector()[I]->isNum()) {
        auto *Num = static_cast<NumExpr*>(Node->getIndexVector()[I].get());
        if (Num->isInt()) {
          int64_t IndexValue = static_cast<IntExpr*>(Num)->getValue();
          if (IndexValue < 0) {
            Diags.Report(DiagKind::Error, "Array index cannot be negative.",
                Node->getIndexVector()[I]->getSourceRange().getStart());
          } else if (static_cast<size_t>(IndexValue) >= 
              ArrayTyPtr->getArraySize()) {
            Diags.Report(DiagKind::Error, "Array index out of bounds.",
                Node->getIndexVector()[I]->getSourceRange().getStart());
          }
        }
      }

      QualType ElemTy = ArrayTyPtr->getElementType();
      ArrayTyPtr = ElemTy.isArrayType()
        ? static_cast<const ArrayType*>(ElemTy.getTypePtr())
        : nullptr;
    }

    QualType ResultTy = Sym->Ty;
    const auto *Cur = static_cast<const ArrayType*>(ResultTy.getTypePtr());
    for (size_t I = 0; I < Node->getIndexVector().size(); ++I) {
      ResultTy = Cur->getElementType();
      if (ResultTy.isArrayType())
        Cur = static_cast<const ArrayType*>(ResultTy.getTypePtr());
    }

    Node->setType(ResultTy); 
  }

  void TypeChecker::visitLetStmt(LetStmt *Node) {
    auto Sym = ST.lookupSymbol(Node->getDeclaredVar()->getName(),
        Node->getScope());
    if(Node->isMut()) Sym->IsMutable = true;
    if (!Sym) {
      Diags.Report(DiagKind::Error, "Variable not found in SymbolTable",
          Node->getSourceRange().getStart());
      return;
    }

    QualType InitializerQualType; 
    QualType FinalType; 

    if (Node->getDeclaredType()) {
      Sym->IsMutable = Node->getDeclaredType()->isMut() || Node->isMut(); 
      QualType DeclaredQualType = resolveType(Node->getDeclaredType());
      if (DeclaredQualType.isNull()) {
        Diags.Report(DiagKind::Error,
            "Unknown type name '" + Node->getDeclaredType()->getName() + "'",
            Node->getDeclaredType()->getSourceRange().getStart());
        return; 
      }

      FinalType = DeclaredQualType;
      QualType OldExpectedType = ExpectedType;
      ExpectedType = DeclaredQualType;

      if (Node->getInitializer()) {
        visit(Node->getInitializer());
        InitializerQualType = Node->getInitializer()->getType();
      }
      ExpectedType = OldExpectedType;

      if (!InitializerQualType.isNull()) {
        if (!Ctx.canImplicitlyConvert(DeclaredQualType, InitializerQualType)) {
          Diags.Report(DiagKind::Error,
              "Type mismatch: cannot initialize variable of type '" + 
              DeclaredQualType.getAsString() +
              "' with value of type '" + 
              InitializerQualType.getAsString() + "'",
              Node->getInitializer()->getSourceRange().getStart());
          return;
        }
      }
    } else {
      if(Node->getInitializer()) {
        visit(Node->getInitializer());
        InitializerQualType = Node->getInitializer()->getType();
      }
      if (!InitializerQualType.isNull()) {
        FinalType = InitializerQualType;
      } else {
        Diags.Report(DiagKind::Error, "Variable declaration requires a type or an initializer",
            Node->getSourceRange().getStart());
        return;
      }
    }

    if (!FinalType.isNull()) {
      Sym->Ty = FinalType;
    }
  }

  void TypeChecker::visitExprStmt(ExprStmt *Node) {
    visit(Node->getExpression());  
  }

  void TypeChecker::visitBinExpr(BinExpr *Node) {
    Lex::TokenKind Op = Node->getOp();

    if (Op == Lex::TokenKind::OP_EQUAL   ||
        Op == Lex::TokenKind::OP_PLUSEQUAL ||
        Op == Lex::TokenKind::OP_MINUSEQUAL) {

      visit(Node->getLHS());

      QualType OldExpectedType = ExpectedType;
      ExpectedType = Node->getLHS()->getType(); 
      visit(Node->getRHS());
      ExpectedType = OldExpectedType;

    } else {
      visit(Node->getLHS());
      visit(Node->getRHS());
    }

    QualType LHSQualType = Node->getLHS()->getType();
    QualType RHSQualType = Node->getRHS()->getType();

    if (LHSQualType.isNull() || RHSQualType.isNull()) {
      Node->setType(QualType());
      return;
    }

    QualType ResultType; 
    bool Error = false;

    switch (Node->getOp()) {
      case Lex::TokenKind::OP_EQUAL:
        if(Ctx.canImplicitlyConvert(RHSQualType, LHSQualType)) {
          ResultType = LHSQualType;
          Node->getRHS()->setType(LHSQualType);
        } else {
          Diags.Report(DiagKind::Error,
              "Operator '=' cannot be applied to different types ('" +
              LHSQualType.getAsString() + "' and '" +
              RHSQualType.getAsString() + "')",
              Node->getSourceRange().getStart());
          Error = true;
        }
        break;
      case Lex::TokenKind::OP_PLUS:
      case Lex::TokenKind::OP_MINUS:
      case Lex::TokenKind::OP_STAR:
      case Lex::TokenKind::OP_SLASH:
      case Lex::TokenKind::OP_MINUSEQUAL:
      case Lex::TokenKind::OP_PLUSEQUAL:
        if (LHSQualType.isNumericType() && RHSQualType.isNumericType()) {
          if (LHSQualType.isFloatingType() || RHSQualType.isFloatingType()) {
            ResultType = LHSQualType; 
          } else {
            ResultType = LHSQualType; 
          }
        } else {
          Diags.Report(DiagKind::Error,
              "Operator '" + std::string(Lex::getTokenString(Node->getOp())) +
              "' cannot be applied to non-numeric types ('" +
              LHSQualType.getAsString() + "' and '" +
              RHSQualType.getAsString() + "')",
              Node->getSourceRange().getStart());
          Error = true;
        }
        break;
      case Lex::TokenKind::OP_EQUALEQUAL:
      case Lex::TokenKind::OP_BANGEQUAL:
      case Lex::TokenKind::OP_LESS:
      case Lex::TokenKind::OP_LESSEQUAL:
      case Lex::TokenKind::OP_GREATER:
      case Lex::TokenKind::OP_GREATEREQUAL:
        if (Ctx.areTypesCompatible(LHSQualType, RHSQualType) ||
            (LHSQualType.isNumericType() && RHSQualType.isNumericType())) {
          ResultType = Ctx.getBoolType();
        } else {
          Diags.Report(DiagKind::Error,
              "Operator '" + std::string(Lex::getTokenString(Node->getOp())) +
              "' cannot compare incompatible types ('" +
              LHSQualType.getAsString() + "' and '" +
              RHSQualType.getAsString() + "')",
              Node->getSourceRange().getStart());
          Error = true;
        }
        break;
      case Lex::TokenKind::OP_AMPAMP:
      case Lex::TokenKind::OP_PIPEPIPE:
        if (LHSQualType.isBooleanType() && RHSQualType.isBooleanType()) {
          ResultType = Ctx.getBoolType();
        } else {
          Diags.Report(DiagKind::Error,
              "Operator '" + std::string(Lex::getTokenString(Node->getOp())) +
              "' requires boolean operands ('" +
              LHSQualType.getAsString() + "' and '" +
              RHSQualType.getAsString() + "')",
              Node->getSourceRange().getStart());
          Error = true;
        }
        break;
      default:
        Diags.Report(DiagKind::Error,
            "Unsupported binary operator '" +
            std::string(Lex::getTokenString(Node->getOp())) + "'",
            Node->getSourceRange().getStart());
        Error = true;
        break;
    }

    if (!Error) {
      Node->setType(ResultType);
    } else {
      Node->setType(QualType()); // Set to null type on error
    }
  }

  void TypeChecker::visitIfStmt(IfStmt *Node) {
    visit(Node->getCondition());

    QualType conditionQualType = Node->getCondition()->getType();
    if (conditionQualType.isNull()) {
      // Error already reported, or type could not be determined.
      return;
    }

    if (!conditionQualType.isBooleanType()) {
      Diags.Report(DiagKind::Error,
          "If condition must be of boolean type, found '" +
          conditionQualType.getAsString() + "'",
          Node->getCondition()->getSourceRange().getStart());
    }

    if (Node->getThenBranch()) {
      visit(Node->getThenBranch());
    }
    if (Node->getElseBranch()) {
      visit(Node->getElseBranch());
    }
  }

  void TypeChecker::visitForStmt(ForStmt *Node) {
    RangeExpr *Range = Node->getRange();
    if (!Range) {
      Diags.Report(DiagKind::Error, "For statement must have a range",
          Node->getSourceRange().getStart());
      return;
    }

    visit(Range);

    Expr *Start = Range->getStart();
    Expr *End = Range->getEnd();

    if (!Start || !End) {
      Diags.Report(DiagKind::Error, "Range expression must have start and end values",
          Range->getSourceRange().getStart());
      return;
    }

    QualType StartType = Start->getType();
    QualType EndType = End->getType();

    QualType IteratorType;

    // For now, assume integer types for range.
    // Future: handle type promotion for mixed types (e.g., float ranges)
    if (StartType.isNull() || EndType.isNull()) {
      Diags.Report(DiagKind::Error, "Range start and end expressions must be typed",
          Range->getSourceRange().getStart());
      // The iterator variable will remain untyped or will be implicitly i32 if not found.
      // We'll proceed to visit the body, but this loop's typing might be off.
      // Default to i32 for now to prevent further errors, if possible.
      IteratorType = Ctx.getI32Type(); // Fallback
    } else if (StartType.isIntegerType() && EndType.isIntegerType()) {
      IteratorType = Ctx.getI32Type(); // Default to i32 for integer ranges
    } else {
      Diags.Report(DiagKind::Error,
          "Range expressions must be integer types, found '" +
          StartType.getAsString() + "' and '" + EndType.getAsString() + "'",
          Range->getSourceRange().getStart());
      // Default to i32 for now to prevent further errors, if possible.
      IteratorType = Ctx.getI32Type(); // Fallback
    }

    // Set the type of the iterator variable in the symbol table.
    VarExpr *IteratorVar = Node->getInit();
    if (!IteratorVar) {
      Diags.Report(DiagKind::Error, "For statement must have an iterator variable",
          Node->getSourceRange().getStart());
      return;
    }

    auto Sym = ST.lookupSymbol(IteratorVar->getName(), IteratorVar->getScope());
    if (Sym) {
      Sym->Ty = IteratorType;
    } else {
      Diags.Report(DiagKind::Error,
          "Iterator variable '" + IteratorVar->getName() + "' not found in symbol table",
          IteratorVar->getSourceRange().getStart());
    }

    if (Node->getBody()) {
      visit(Node->getBody());
    }
  }


  void TypeChecker::visitWhileStmt(WhileStmt *Node) {
    visit(Node->getCondition());

    QualType ConditionQualType = Node->getCondition()->getType();
    if (ConditionQualType.isNull()) {
      return;
    }

    if (!ConditionQualType.isBooleanType()) {
      Diags.Report(DiagKind::Error,
          "While condition must be of boolean type, found '" +
          ConditionQualType.getAsString() + "'",
          Node->getCondition()->getSourceRange().getStart());
    }

    if (Node->getBody()) {
      visit(Node->getBody());
    }
  }

  void TypeChecker::visitFuncDecl(FuncDecl *Node) {
    QualType OldFunctionReturnType = CurrentFunctionReturnType;
    CurrentFunctionReturnType = QualType(); 
    std::vector<QualType> ParamTypes;
    if(!Node->getParams().empty()) {
      for (const auto &Param : Node->getParams()) {
        if (Param.ParamType) {
          QualType ParamQualType = resolveType(Param.ParamType.get());
          if (ParamQualType.isNull()) {
            Diags.Report(DiagKind::Error,
                "Unknown type name for parameter '" + Param.ParamName->getName() 
                + "'", Param.ParamType->getSourceRange().getStart());
          } 
          else {
            auto Sym = ST.lookupSymbol(Param.ParamName->getName(), 
                Param.ParamName->getScope());
            if (Sym) {
              Sym->Ty = ParamQualType;
              ParamTypes.emplace_back(Sym->Ty);
            } else {
              Diags.Report(DiagKind::Error,
                  "Parameter '" + Param.ParamName->getName() + 
                  "' not found in symbol table (TypeChecker)",
                  Param.ParamType->getSourceRange().getStart());
            }
          }
        } else {
          Diags.Report(DiagKind::Error,
              "Parameter '" + Param.ParamName->getName() + 
              "' requires an explicit type", Node->getSourceRange().getStart());
        }
      }
    }
    QualType FuncReturnQualType;
    if (Node->getReturnType()) {
      FuncReturnQualType = resolveType(Node->getReturnType());
      if (FuncReturnQualType.isNull()) {
        Diags.Report(DiagKind::Error, "Unknown return type name for function '" 
            + Node->getFuncName()->getName() + "'", 
            Node->getReturnType()->getSourceRange().getStart());
      }
      CurrentFunctionReturnType = FuncReturnQualType;
    } else {
      FuncReturnQualType = Ctx.getUnitType();
    }

    QualType FunctionType = Ctx.getFunctionType(FuncReturnQualType,ParamTypes); 
    auto Sym = ST.lookupSymbol(Node->getFuncName()->getName());
    if (Sym) Sym->Ty = FunctionType;
    if (Node->getBody()) {
      visit(Node->getBody());
    }
    CurrentFunctionReturnType = OldFunctionReturnType;
  }

  void TypeChecker::visitFunCall(FunCall *Node) {
    std::string Name = std::string(Node->getFuncName()->getName());
    QualType FuncExpType;
    Symbol* SymbolInfo = ST.lookupSymbol(Name);
    if(SymbolInfo) {
      FuncExpType = SymbolInfo->Ty;
    } else {
      Diags.Report(DiagKind::Error, "Function not defined or incorrectly named");
      return;
    }
    if (FuncExpType.isNull()) {
      return;
    }
    QualType ReturnExpType = FuncExpType.getReturnType();
    Node->setType(ReturnExpType);
    std::vector<QualType> ParamsExpType = FuncExpType.getParamsType();
    
    //FIXME :: Currently this matches all the params even if some is defined.
    //While not ideal it works and i dont have to care about the ordering of 
    //params and assignment in the function declaration.
    const std::vector<std::unique_ptr<Expr>>& ParamCallType = Node->getParams();
    for(int I = 0; I < ParamCallType.size(); ++I) {
      visit(ParamCallType[I].get());
      if(ParamCallType[I]->getType() != ParamsExpType[I]) {
        Diags.Report(DiagKind::Error, "Function " + Name + " expected Type "
            + ParamsExpType[I].getAsString() + " but got " + 
            ParamCallType[I]->getType().getAsString());
        break;
      }
    }
  }

  void TypeChecker::visitReturnStmt(ReturnStmt *Node) {
    if (Node->getReturnValue()) {
      visit(Node->getReturnValue());
    }

    QualType ActualQualType; 
    if (Node->getReturnValue()) {
      ActualQualType = Node->getReturnValue()->getType();
    }

    QualType ExpectedQualType = CurrentFunctionReturnType;

    if (!ActualQualType.isNull() && !ExpectedQualType.isNull()) {
      if (!Ctx.areTypesCompatible(ExpectedQualType, ActualQualType)) {
        Diags.Report(DiagKind::Error,
          "Return type mismatch: expected '" + ExpectedQualType.getAsString() +
            "', found '" + ActualQualType.getAsString() + "'",
            Node->getSourceRange().getStart());
      }
    } 
    else if (ActualQualType.isNull() && !ExpectedQualType.isNull()) {
      Diags.Report(DiagKind::Error,
          "Function expected a return value of type '" 
          + ExpectedQualType.getAsString() + "', but found none",
          Node->getSourceRange().getStart());
    } 
    else if (!ActualQualType.isNull() && ExpectedQualType.isNull()) {
      Diags.Report(DiagKind::Error,
          "Function expected no return value, but found one of type '" +
          ActualQualType.getAsString() + "'",
          Node->getSourceRange().getStart());
    }
  }

} // namespace trsc
