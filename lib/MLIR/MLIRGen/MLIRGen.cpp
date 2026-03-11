
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/BuiltinTypeInterfaces.h"
#include "mlir/IR/Operation.h"
#include "mlir/IR/Types.h"
#include "mlir/IR/Value.h"
#include "mlir/Support/LLVM.h"
#include "trsc/AST/AST.h"
#include "trsc/AST/QualType.h"
#include "trsc/AST/ASTContext.h"
#include "trsc/MLIR/TrscMLIRGen.h"
#include "trsc/Sema/SymbolTable.h"

#include "mlir/IR/Builders.h"
#include "mlir/IR/MLIRContext.h"
#include "mlir/IR/OwningOpRef.h"
#include "mlir/IR/Verifier.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/ControlFlow/IR/ControlFlow.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/Support/Casting.h"
#include <vector>

using namespace trsc; 
using namespace mlir;

MLIRGen::MLIRGen(mlir::MLIRContext &MLIRCtx, trsc::ASTContext &ASTCtx, 
    trsc::SymbolTable &ST) : MLIRCtx(MLIRCtx), ASTCtx(ASTCtx), ST(ST), 
  Builder(&MLIRCtx) {
    mlir::DialectRegistry Registry;
    Registry.insert<mlir::func::FuncDialect>();
    Registry.insert<mlir::cf::ControlFlowDialect>();
    Registry.insert<mlir::memref::MemRefDialect>();
    Registry.insert<mlir::arith::ArithDialect>();
    Registry.insert<mlir::scf::SCFDialect>();
    MLIRCtx.appendDialectRegistry(Registry);
    MLIRCtx.loadAllAvailableDialects();
  }

mlir::Type MLIRGen::ToMLIRType(QualType T) {
  if (T.isNull() || T.isUnitType()) {
    return Builder.getNoneType();
  }
  if (T.isIntegerType()) {
    return Builder.getI32Type();
  }
  if (T.isFloatingType()) {
    return Builder.getF64Type();
  }
  if (T.isBooleanType()) {
    return Builder.getI1Type();
  }
  return Builder.getNoneType();
}

mlir::MemRefType MLIRGen::ToMemRefType(QualType T) {
    return mlir::MemRefType::get({}, ToMLIRType(T));
  }

llvm::APFloat MLIRGen::ToAPFloat(double D) {
  return llvm::APFloat(D); 
}

bool MLIRGen::isLValue(Expr *E) {
  return static_cast<VarExpr*>(E) != nullptr;
}

mlir::Value MLIRGen::getLValueMemRef(Expr *E) {
  if (auto *VE = static_cast<VarExpr*>(E)) {
    Symbol *Sym = ST.lookupSymbol(VE->getName(), VE->getScope());
    
    mlir::Operation* GenericOp = Sym->Op.dyn_cast<mlir::Operation*>();
    auto AllocaOp = llvm::dyn_cast<mlir::memref::AllocaOp>(GenericOp);
    
    return AllocaOp.getMemref();
  }
  
  return mlir::Value();
}

void MLIRGen::genStmt(Stmt *S) {
  if (!S)
    return;

  switch (S->getASTNodeKind()) {
    case ASTNodeKind::ASTK_FUNCDECL:
      genFuncDecl(static_cast<FuncDecl*>(S));
      break;
    case ASTNodeKind::ASTK_LETSTMT:
      genLetStmt(static_cast<LetStmt*>(S));
      break;
    case ASTNodeKind::ASTK_IFSTMT:
      genIfStmt(static_cast<IfStmt*>(S));
      break;
    case ASTNodeKind::ASTK_RETURNSTMT:
      genReturnStmt(static_cast<ReturnStmt*>(S));
      break;
    case ASTNodeKind::ASTK_BLOCKSTMT:
      genBlockStmt(static_cast<BlockStmt*>(S));
      break;
    case ASTNodeKind::ASTK_EXPRSTMT:
      genExprStmt(static_cast<ExprStmt*>(S));
      break;
    case ASTNodeKind::ASTK_FORSTMT:
      genForStmt(static_cast<ForStmt*>(S));
      break;
    case ASTNodeKind::ASTK_WHILESTMT:
      genWhileStmt(static_cast<WhileStmt*>(S));
      break;
    default:
      llvm_unreachable("Unhandled statement type");
  }
}

void MLIRGen::genProgram(Program *Node) {
  for(auto& S: Node->getStatements()) {
    genStmt(S.get());
  }
}

void MLIRGen::genBlockStmt(BlockStmt *Node) {
  for (const auto &S : Node->getStatements()) {
    genStmt(S.get());
  }
}

void MLIRGen::genLetStmt(LetStmt *Node) {
  mlir::Value InitValue;
  if (Node->getInitializer()) {
    InitValue = visit(Node->getInitializer());
  }
  Symbol* Sym = ST.lookupSymbol(Node->getDeclaredVar()->getName(), 
      Node->getScope());
  QualType VarTy = Sym->Ty; 

  mlir::memref::AllocaOp VarAlloca;
  {
    mlir::OpBuilder::InsertionGuard Guard(Builder);
    Builder.setInsertionPointToStart(this->CurrentEntryBlock);
    VarAlloca = mlir::memref::AllocaOp::create(Builder, 
        Builder.getUnknownLoc(), 
        ToMemRefType(VarTy));  
  }
  Sym->setAlloca(VarAlloca);
  mlir::memref::StoreOp::create(Builder, 
      Builder.getUnknownLoc(), 
      InitValue,
      VarAlloca.getResult());
}

mlir::Value MLIRGen::visitIntExpr(IntExpr *Node) {
  auto IntOp = mlir::arith::ConstantIntOp::create(Builder, 
      Builder.getUnknownLoc(), 
      ToMLIRType(Node->getType()), 
      Node->getValue());
  return IntOp.getResult();
}

mlir::Value MLIRGen::visitFloatExpr(FloatExpr *Node) {
  auto FloatOp = mlir::arith::ConstantFloatOp::create(Builder,
      Builder.getUnknownLoc(),
      llvm::dyn_cast<mlir::FloatType>( ToMLIRType(Node->getType())),
      ToAPFloat(Node->getValue()));
  return FloatOp;
}

mlir::Value MLIRGen::visitBinExpr(BinExpr *Node) {
  // Visit left and right expressions to get their MLIR values
  mlir::Value LHS = visit(Node->getLHS());
  mlir::Value RHS = visit(Node->getRHS());

  if (!LHS || !RHS) {
    llvm::errs() << "Error: Failed to generate operands for binary expression\n";
    return mlir::Value();
  }

  auto Loc = Builder.getUnknownLoc();
  Lex::TokenKind Op = Node->getOp();

  // Get the result type from the typed AST (semantic analysis already determined this)
  QualType ResultTy = Node->getType();

  // Determine if we're working with integers or floats
  bool IsFloat = ResultTy.isFloatingType();
  bool IsBool = ResultTy.isBooleanType();
  bool IsSigned = ResultTy.isSignedIntegerType();

  switch (Op) {
    // ========================================================================
    // Arithmetic Operators
    // ========================================================================
    case Lex::TokenKind::OP_PLUS:
      if (IsFloat) {
        return mlir::arith::AddFOp::create(Builder, Loc, LHS, RHS);
      } else {
        return mlir::arith::AddIOp::create(Builder, Loc, LHS, RHS);
      }

    case Lex::TokenKind::OP_MINUS:
      if (IsFloat) {
        return mlir::arith::SubFOp::create(Builder, Loc, LHS, RHS);
      } else {
        return mlir::arith::SubIOp::create(Builder, Loc, LHS, RHS);
      }

    case Lex::TokenKind::OP_STAR:
      if (IsFloat) {
        return mlir::arith::MulFOp::create(Builder, Loc, LHS, RHS);
      } else {
        return mlir::arith::MulIOp::create(Builder, Loc, LHS, RHS);
      }

    case Lex::TokenKind::OP_SLASH:
      if (IsFloat) {
        return mlir::arith::DivFOp::create(Builder, Loc, LHS, RHS);
      } else if (IsSigned){
        return mlir::arith::DivSIOp::create(Builder, Loc, LHS, RHS);
      } else {
        return mlir::arith::DivUIOp::create(Builder, Loc, LHS, RHS);
      }

    // ========================================================================
    // Comparison Operators (return i1/bool)
    // ========================================================================
    case Lex::TokenKind::OP_EQUALEQUAL:
      if (IsFloat) {
        return mlir::arith::CmpFOp::create(Builder,
            Loc, 
            mlir::arith::CmpFPredicate::OEQ,  // Ordered equal
            LHS, RHS);
      } else {
        return mlir::arith::CmpIOp::create(Builder,
            Loc,
            mlir::arith::CmpIPredicate::eq,
            LHS, RHS);
      }

    case Lex::TokenKind::OP_BANGEQUAL:
      if (IsFloat) {
        return mlir::arith::CmpFOp::create(Builder,
            Loc,
            mlir::arith::CmpFPredicate::ONE,  // Ordered not equal
            LHS, RHS);
      } else {
        return mlir::arith::CmpIOp::create(Builder,
            Loc,
            mlir::arith::CmpIPredicate::ne,
            LHS, RHS);
      }

    case Lex::TokenKind::OP_LESS:
      if (IsFloat) {
        return mlir::arith::CmpFOp::create(Builder,
            Loc,
            mlir::arith::CmpFPredicate::OLT,  // Ordered less than
            LHS, RHS);
      } else {
        return mlir::arith::CmpIOp::create(Builder,
            Loc,
            mlir::arith::CmpIPredicate::slt,  // Signed less than
            LHS, RHS);
      }

    case Lex::TokenKind::OP_LESSEQUAL:
      if (IsFloat) {
        return mlir::arith::CmpFOp::create(Builder,
            Loc,
            mlir::arith::CmpFPredicate::OLE,  // Ordered less or equal
            LHS, RHS);
      } else {
        return mlir::arith::CmpIOp::create(Builder,
            Loc,
            mlir::arith::CmpIPredicate::sle,  // Signed less or equal
            LHS, RHS);
      }

    case Lex::TokenKind::OP_GREATER:
      if (IsFloat) {
        return mlir::arith::CmpFOp::create(Builder,
            Loc,
            mlir::arith::CmpFPredicate::OGT,  // Ordered greater than
            LHS, RHS);
      } else {
        return mlir::arith::CmpIOp::create(Builder,
            Loc,
            mlir::arith::CmpIPredicate::sgt,  // Signed greater than
            LHS, RHS);
      }

    case Lex::TokenKind::OP_GREATEREQUAL:
      if (IsFloat) {
        return mlir::arith::CmpFOp::create(Builder,
            Loc,
            mlir::arith::CmpFPredicate::OGE,  // Ordered greater or equal
            LHS, RHS);
      } else {
        return mlir::arith::CmpIOp::create(Builder,
            Loc,
            mlir::arith::CmpIPredicate::sge,  // Signed greater or equal
            LHS, RHS);
      }

    // ========================================================================
    // Logical Operators (boolean operations)
    // ========================================================================
    case Lex::TokenKind::OP_AMPAMP:  // &&
                                     // LHS && RHS
      return mlir::arith::AndIOp::create(Builder, Loc, LHS, RHS);

    case Lex::TokenKind::OP_PIPEPIPE:  // ||
                                       // LHS || RHS
      return mlir::arith::OrIOp::create(Builder, Loc, LHS, RHS);

    default:
      llvm::errs() << "Error: Unhandled binary operator: " 
        << Lex::getTokenName(Op) << "\n";
      return mlir::Value();
  }
}

mlir::Value MLIRGen::visitBoolExpr(BoolExpr *Node) {
  auto BoolOp = mlir::arith::ConstantIntOp::create(Builder, 
      Builder.getUnknownLoc(),
      ToMLIRType(Node->getType()),
      Node->getValue());
  return BoolOp;
}

mlir::Value MLIRGen::visitVarExpr(VarExpr *Node) {
  Symbol *Sym = ST.lookupSymbol(Node->getName(), Node->getScope());
  mlir::Operation* genericOp = Sym->Op.dyn_cast<mlir::Operation*>();
  auto allocaOp = llvm::dyn_cast<mlir::memref::AllocaOp>(genericOp);

  mlir::memref::LoadOp loadOp = mlir::memref::LoadOp::create(Builder,
      Builder.getUnknownLoc(),
      allocaOp.getMemref()); 
  return loadOp;
}

mlir::Value MLIRGen::visitFunCall(FunCall *Node) {
  auto Loc = Builder.getUnknownLoc();

  std::string FuncName = Node->getFuncName()->getName();

  Symbol *FuncSym = ST.lookupSymbol(FuncName);
  if (!FuncSym) {
    llvm::errs() << "Error: Function '" << FuncName 
      << "' not found in symbol table\n";
    return mlir::Value();
  }

  mlir::Operation* genericOp = FuncSym->Op.dyn_cast<mlir::Operation*>();

  mlir::func::FuncOp funcOp = llvm::dyn_cast<mlir::func::FuncOp>(genericOp);

  llvm::ArrayRef<mlir::Type> ResultTypes = funcOp.getResultTypes();

  QualType FuncTy = FuncSym->Ty;
  if (!FuncTy.isFunctionType()) {
    llvm::errs() << "Error: Symbol '" << FuncName << "' is not a function\n";
    return mlir::Value();
  }

  llvm::SmallVector<mlir::Value, 4> Args;
  for (const auto &ArgExpr : Node->getParams()) {
    mlir::Value ArgValue = visit(ArgExpr.get());
    if (!ArgValue) {
      llvm::errs() << "Error: Failed to generate argument for function call\n";
      return mlir::Value();
    }
    Args.push_back(ArgValue);
  }

  auto CallOp = mlir::func::CallOp::create(Builder, Loc, funcOp, Args);
  
  // If the function returns a value, return it
  // Otherwise, return an empty mlir::Value (for void functions)
  if (ResultTypes.empty()) {
    return mlir::Value();
  } else {
    return CallOp.getResult(0);
  }
}

void MLIRGen::genParams(const std::vector<FuncDecl::Param>& Params) { 
  auto Loc = Builder.getUnknownLoc();

  for (size_t i = 0; i < Params.size(); ++i) {
    const auto& Param = Params[i];

    Symbol* ParamSym = ST.lookupSymbol(Param.ParamName->getName(), 
        Param.ParamName->getScope());

    if (!ParamSym) {
      llvm::errs() << "Error: Parameter '" << Param.ParamName->getName() 
        << "' not found in symbol table\n";
      continue;
    }

    QualType ParamType = ParamSym->Ty;

    auto ParamAlloca = mlir::memref::AllocaOp::create(Builder, 
        Loc, ToMemRefType(ParamType));

    // Get the corresponding block argument (function parameter value)
    mlir::Value BlockArg = CurrentEntryBlock->getArgument(i);

    // Store the block argument into the alloca
    mlir::memref::StoreOp::create(Builder, Loc, BlockArg, ParamAlloca.getResult());

    // Update symbol table to point to this alloca
    ParamSym->setAlloca(ParamAlloca); 
  }
}

void MLIRGen::genFuncDecl(FuncDecl *Node) {
  auto Loc = Builder.getUnknownLoc(); 
  Symbol* Sym = ST.lookupSymbol(Node->getFuncName()->getName());
  if (!Sym) return;

  QualType FuncTy = Sym->Ty;
  if (!FuncTy.isFunctionType()) return;

  std::vector<mlir::Type> ArgTypes;
  for (const auto& ParamTy : FuncTy.getParamsType()) {
    ArgTypes.push_back(ToMLIRType(ParamTy));
  }
  
  QualType RetT = FuncTy.getReturnType();
  std::vector<mlir::Type> ResultTypes;
  if (!RetT.isUnitType()) {
    ResultTypes.push_back(ToMLIRType(RetT));
  }

  auto FuncType = Builder.getFunctionType(ArgTypes, ResultTypes);

  auto funcOp = mlir::func::FuncOp::create(
      Builder, Loc, Node->getFuncName()->getName(), FuncType);
  Sym->setFunc(funcOp);

  auto *EntryBlock = funcOp.addEntryBlock();
  this->CurrentEntryBlock = EntryBlock;

  mlir::OpBuilder::InsertionGuard Guard(Builder);
  Builder.setInsertionPointToStart(CurrentEntryBlock);

  if(!Node->getParams().empty()) { genParams(Node->getParams()); }

  if(Node->getBody()) {
    genStmt(Node->getBody());
  }

  if (ResultTypes.empty()) {
    if (EntryBlock->empty() || 
        !EntryBlock->back().hasTrait<mlir::OpTrait::IsTerminator>()) {
      mlir::func::ReturnOp::create(Builder, Builder.getUnknownLoc());
    }
  }

  this->CurrentEntryBlock = nullptr;
}

void MLIRGen::genReturnStmt(ReturnStmt *Node) {
  auto Loc = Builder.getUnknownLoc();

  // No return value (void return)
  if (!Node->getReturnValue()) {
    mlir::func::ReturnOp::create(Builder, Loc);
    return;
  }

  // Has return value
  mlir::Value RetValue = visit(Node->getReturnValue());

  if (!RetValue) {
    llvm::errs() << "Error: Failed to generate return value expression\n";
    return;
  }

  mlir::func::ReturnOp::create(Builder, Loc, RetValue);
}

void MLIRGen::genIfStmt(IfStmt *Node) {
  auto Loc = Builder.getUnknownLoc();

  // Generate condition (must be i1 type)
  mlir::Value Condition = visit(Node->getCondition());

  if (!Condition) {
    llvm::errs() << "Error: Failed to generate condition for if statement\n";
    return;
  }

  // Verify condition is i1 type
  if (!Condition.getType().isInteger(1)) {
    llvm::errs() << "Error: If condition must be of type i1 (bool), got: "
      << Condition.getType() << "\n";
    return;
  }

  // Create scf.if operation
  auto IfOp = mlir::scf::IfOp::create(Builder,
      Loc,
      Condition,
      /*withElseRegion=*/Node->getElseBranch() != nullptr);

  // Generate "then" region
  {
    mlir::OpBuilder::InsertionGuard Guard(Builder);
    Builder.setInsertionPointToStart(&IfOp.getThenRegion().front());

    genStmt(Node->getThenBranch());

    // Add scf.yield if no terminator exists
    mlir::Block *ThenBlock = &IfOp.getThenRegion().front();
    if (ThenBlock->empty() || 
        !ThenBlock->back().hasTrait<mlir::OpTrait::IsTerminator>()) {
      mlir::scf::YieldOp::create(Builder, Loc);
    }
  }

  // Generate "else" region (if it exists)
  if (Node->getElseBranch()) {
    mlir::OpBuilder::InsertionGuard Guard(Builder);
    Builder.setInsertionPointToStart(&IfOp.getElseRegion().front());

    genStmt(Node->getElseBranch());

    // Add scf.yield if no terminator exists
    mlir::Block *ElseBlock = &IfOp.getElseRegion().front();
    if (ElseBlock->empty() || 
        !ElseBlock->back().hasTrait<mlir::OpTrait::IsTerminator>()) {
      mlir::scf::YieldOp::create(Builder, Loc);
    }
  }
}

void MLIRGen::genAssignment(BinExpr *Node) {
  auto Loc = Builder.getUnknownLoc();

  // 1. Verify LHS is an lvalue
  if (!isLValue(Node->getLHS())) {
    llvm::errs() << "Error: Cannot assign to non-lvalue\n";
    return;
  }

  // 2. Get the memref for the variable
  mlir::Value LHSMemRef = getLValueMemRef(Node->getLHS());

  mlir::Value ValueToStore;
  Lex::TokenKind Op = Node->getOp();

  if (Op == Lex::TokenKind::OP_EQUAL) {
    // Simple: a = b
    ValueToStore = visit(Node->getRHS());

  } else {
    // Compound: a += b or a -= b

    mlir::Value CurrentValue = mlir::memref::LoadOp::create(Builder,
        Loc, LHSMemRef);

    // Evaluate RHS
    mlir::Value RHSValue = visit(Node->getRHS());

    // Perform operation
    QualType ResultTy = Node->getType();
    bool IsFloat = ResultTy.isFloatingType();

    if (Op == Lex::TokenKind::OP_PLUSEQUAL) {
      ValueToStore = IsFloat 
        ? mlir::arith::AddFOp::create(Builder, Loc, CurrentValue, RHSValue).getResult()
        : mlir::arith::AddIOp::create(Builder, Loc, CurrentValue, RHSValue).getResult();

    } else if (Op == Lex::TokenKind::OP_MINUSEQUAL) {
      ValueToStore = IsFloat
        ? mlir::arith::SubFOp::create(Builder, Loc, CurrentValue, RHSValue).getResult()
        : mlir::arith::SubIOp::create(Builder, Loc, CurrentValue, RHSValue).getResult();
    }
  }

  // 3. Store the result
  mlir::memref::StoreOp::create(Builder, Loc, ValueToStore, LHSMemRef);
}

void MLIRGen::genExprStmt(ExprStmt *Node) {
  Expr *E = Node->getExpression();

  if (!E) return;

  // Special handling for assignment operators
  if (auto *BE = static_cast<BinExpr*>(E)) {
    Lex::TokenKind Op = BE->getOp();

    if (Op == Lex::TokenKind::OP_EQUAL ||
        Op == Lex::TokenKind::OP_PLUSEQUAL ||
        Op == Lex::TokenKind::OP_MINUSEQUAL) {
      genAssignment(BE);
      return;
    }
  }

  // For other expressions (function calls, etc.), just evaluate
  visit(E);
}

void MLIRGen::genForStmt(ForStmt *Node) {
  auto loc = Builder.getUnknownLoc();

  // 1. Generate loop bounds and step
  mlir::Value lbValue = visit(Node->getRange()->getStart());
  mlir::Value ubValue = visit(Node->getRange()->getEnd());
  mlir::Value lb = mlir::arith::IndexCastOp::create(Builder,
      loc, Builder.getIndexType(), lbValue);
  mlir::Value ub = mlir::arith::IndexCastOp::create(Builder,
      loc, Builder.getIndexType(), ubValue);
  mlir::Value step = mlir::arith::ConstantIndexOp::create(Builder, loc, 1);

  // 2. Create the ForOp
  // Note: If you don't have loop-carried variables, iterArgs is empty
  auto forOp = mlir::scf::ForOp::create(Builder, loc, lb, ub, step);

  // 3. Setup the Body
  mlir::OpBuilder::InsertionGuard guard(Builder);
  Builder.setInsertionPointToStart(forOp.getBody());

  genStmt(Node->getBody());

  mlir::Block *bodyBlock = forOp.getBody();
    if (bodyBlock->empty() || 
        !bodyBlock->back().hasTrait<mlir::OpTrait::IsTerminator>()) {
      mlir::scf::YieldOp::create(Builder, loc);
    }
}

void MLIRGen::genWhileStmt(WhileStmt *Node) {
  auto loc = Builder.getUnknownLoc();

  // 1. Create WhileOp. 'operands' are initial loop-carried values (often empty)
  auto whileOp = mlir::scf::WhileOp::create(Builder, loc, mlir::TypeRange{}, 
      mlir::ValueRange{});
  {
    mlir::OpBuilder::InsertionGuard guard(Builder);
    mlir::Region &beforeRegion = whileOp.getBefore();
    mlir::Block *beforeBlock = Builder.createBlock(&beforeRegion);
    Builder.setInsertionPointToStart(beforeBlock);

    mlir::Value condition = visit(Node->getCondition());
    // scf.condition takes the boolean and any values to pass to the body
    if (!condition) {
      llvm::errs() << "Error: Failed to generate while loop condition\n";
      return;
    }
    mlir::scf::ConditionOp::create(Builder, loc, condition, 
        beforeBlock->getArguments());
  }

  {
    mlir::OpBuilder::InsertionGuard guard(Builder);
    mlir::Region &afterRegion = whileOp.getAfter();
    mlir::Block *afterBlock = Builder.createBlock(&afterRegion);
    Builder.setInsertionPointToStart(afterBlock);

    genStmt(Node->getBody());

    // 4. Add scf.yield if no terminator exists
    if (afterBlock->empty() || 
        !afterBlock->back().hasTrait<mlir::OpTrait::IsTerminator>()) {
      mlir::scf::YieldOp::create(Builder, loc);
    }
  }
}

mlir::OwningOpRef<mlir::ModuleOp> MLIRGen::genModule(trsc::Program &Prog) {
  Module = mlir::ModuleOp::create(mlir::UnknownLoc::get(&MLIRCtx));
  Builder.setInsertionPointToEnd(Module.getBody());
  
  genProgram(&Prog);

  if (failed(mlir::verify(Module))) {
    Module.emitError("Module verification failed.");
    return nullptr;
  }

  return mlir::OwningOpRef<mlir::ModuleOp>(Module);
}
