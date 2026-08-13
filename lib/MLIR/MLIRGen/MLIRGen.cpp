#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/Linalg/IR/Linalg.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/Verifier.h"

#include "mlir/Dialect/GPU/IR/GPUDialect.h"
#include "mlir/Dialect/LLVMIR/LLVMDialect.h"
#include "mlir/Dialect/LLVMIR/NVVMDialect.h"
#include "mlir/Dialect/Vector/IR/VectorOps.h"
#include "mlir/Target/LLVM/NVVM/Target.h"

#include "mlir/Conversion/ArithToLLVM/ArithToLLVM.h"
#include "mlir/Conversion/ComplexToLLVM/ComplexToLLVM.h"
#include "mlir/Conversion/ControlFlowToLLVM/ControlFlowToLLVM.h"
#include "mlir/Conversion/FuncToLLVM/ConvertFuncToLLVM.h"
#include "mlir/Conversion/GPUCommon/GPUToLLVM.h"
#include "mlir/Conversion/GPUToNVVM/GPUToNVVM.h"
#include "mlir/Conversion/IndexToLLVM/IndexToLLVM.h"
#include "mlir/Conversion/MathToLLVM/MathToLLVM.h"
#include "mlir/Conversion/MemRefToLLVM/MemRefToLLVM.h"
#include "mlir/Conversion/NVVMToLLVM/NVVMToLLVM.h"
#include "mlir/Conversion/UBToLLVM/UBToLLVM.h"
#include "mlir/Conversion/VectorToLLVM/ConvertVectorToLLVM.h"

#include "mlir/Target/LLVMIR/Dialect/Builtin/BuiltinToLLVMIRTranslation.h"
#include "mlir/Target/LLVMIR/Dialect/GPU/GPUToLLVMIRTranslation.h"
#include "mlir/Target/LLVMIR/Dialect/LLVMIR/LLVMToLLVMIRTranslation.h"
#include "mlir/Target/LLVMIR/Dialect/NVVM/NVVMToLLVMIRTranslation.h"

#include "trsc/AST/ASTContext.h"
#include "trsc/MLIR/TrscDialect.h"
#include "trsc/MLIR/TrscMLIRGen.h"
#include "trsc/Sema/SymbolTable.h"

using namespace trsc;

MLIRGen::MLIRGen(mlir::MLIRContext &MLIRCtx, trsc::ASTContext &ASTCtx,
                 trsc::SymbolTable &ST)
    : MLIRCtx(MLIRCtx), ASTCtx(ASTCtx), ST(ST), Builder(&MLIRCtx) {
  mlir::DialectRegistry Registry;
  Registry.insert<mlir::func::FuncDialect>();
  Registry.insert<mlir::memref::MemRefDialect>();
  Registry.insert<mlir::arith::ArithDialect>();
  Registry.insert<mlir::scf::SCFDialect>();
  Registry.insert<mlir::linalg::LinalgDialect>();
  Registry.insert<mlir::gpu::GPUDialect>();
  Registry.insert<mlir::LLVM::LLVMDialect>();
  Registry.insert<mlir::NVVM::NVVMDialect>();
  Registry.insert<mlir::vector::VectorDialect>();
  Registry.insert<mlir::trscd::TrscDialect>();

  mlir::arith::registerConvertArithToLLVMInterface(Registry);
  mlir::registerConvertComplexToLLVMInterface(Registry);
  mlir::cf::registerConvertControlFlowToLLVMInterface(Registry);
  mlir::registerConvertFuncToLLVMInterface(Registry);
  mlir::index::registerConvertIndexToLLVMInterface(Registry);
  mlir::registerConvertMathToLLVMInterface(Registry);
  mlir::registerConvertMemRefToLLVMInterface(Registry);
  mlir::registerConvertNVVMToLLVMInterface(Registry);
  mlir::ub::registerConvertUBToLLVMInterface(Registry);
  mlir::vector::registerConvertVectorToLLVMInterface(Registry);
  mlir::gpu::registerConvertGpuToLLVMInterface(Registry);
  mlir::NVVM::registerConvertGpuToNVVMInterface(Registry);

  mlir::NVVM::registerNVVMTargetInterfaceExternalModels(Registry);

  mlir::registerBuiltinDialectTranslation(Registry);
  mlir::registerLLVMDialectTranslation(Registry);
  mlir::registerGPUDialectTranslation(Registry);
  mlir::registerNVVMDialectTranslation(Registry);
  mlir::gpu::registerOffloadingLLVMTranslationInterfaceExternalModels(Registry);

  MLIRCtx.appendDialectRegistry(Registry);
  MLIRCtx.loadAllAvailableDialects();
}

namespace {

enum class NumKind { SignedInt = 0, UnsignedInt = 1, Float = 2 };

NumKind getNumKind(mlir::Type T) {
  if (T.isF16() || T.isF32() || T.isF64() || T.isBF16())
    return NumKind::Float;
  if (auto IT = llvm::dyn_cast<mlir::IntegerType>(T)) {
    if (IT.isSigned() || IT.isSignless())
      return NumKind::SignedInt;
    return NumKind::UnsignedInt;
  }
  llvm_unreachable("Unknown numeric type");
}

// SS: Signed -> Signed
mlir::Value conv_SS(mlir::Value From, mlir::Type F, mlir::Type T,
                    mlir::OpBuilder &Builder) {
  auto Loc = Builder.getUnknownLoc();
  unsigned FW = F.getIntOrFloatBitWidth();
  unsigned TW = T.getIntOrFloatBitWidth();
  if (FW == TW)
    return From;
  if (FW < TW)
    return mlir::arith::ExtSIOp::create(Builder, Loc, T, From);
  return mlir::arith::TruncIOp::create(Builder, Loc, T, From);
}

// UU: Unsigned -> Unsigned
mlir::Value conv_UU(mlir::Value From, mlir::Type F, mlir::Type T,
                    mlir::OpBuilder &Builder) {
  auto Loc = Builder.getUnknownLoc();
  unsigned FW = F.getIntOrFloatBitWidth();
  unsigned TW = T.getIntOrFloatBitWidth();
  if (FW == TW)
    return From;
  if (FW < TW)
    return mlir::arith::ExtUIOp::create(Builder, Loc, T, From);
  return mlir::arith::TruncIOp::create(Builder, Loc, T, From);
}

// FF: Float -> Float
mlir::Value conv_FF(mlir::Value From, mlir::Type F, mlir::Type T,
                    mlir::OpBuilder &Builder) {
  auto Loc = Builder.getUnknownLoc();
  unsigned FW = F.getIntOrFloatBitWidth();
  unsigned TW = T.getIntOrFloatBitWidth();
  if (FW == TW)
    return From;
  if (FW < TW)
    return mlir::arith::ExtFOp::create(Builder, Loc, T, From);
  return mlir::arith::TruncFOp::create(Builder, Loc, T, From);
}

// SU: Signed -> Unsigned
mlir::Value conv_SU(mlir::Value From, mlir::Type F, mlir::Type T,
                    mlir::OpBuilder &Builder) {
  auto Loc = Builder.getUnknownLoc();
  unsigned FW = F.getIntOrFloatBitWidth();
  unsigned TW = T.getIntOrFloatBitWidth();
  mlir::Value Widened = From;
  if (FW < TW)
    Widened = mlir::arith::ExtSIOp::create(Builder, Loc, T, From);
  else if (FW > TW) {
    Widened = mlir::arith::TruncIOp::create(Builder, Loc, T, From);
  }
  return Widened;
}

// US: Unsigned -> Signed
mlir::Value conv_US(mlir::Value From, mlir::Type F, mlir::Type T,
                    mlir::OpBuilder &Builder) {
  auto Loc = Builder.getUnknownLoc();
  unsigned FW = F.getIntOrFloatBitWidth();
  unsigned TW = T.getIntOrFloatBitWidth();
  mlir::Value Widened = From;
  if (FW < TW)
    Widened = mlir::arith::ExtUIOp::create(Builder, Loc, T, From);
  else if (FW > TW) {
    Widened = mlir::arith::TruncIOp::create(Builder, Loc, T, From);
  }
  return Widened;
}

// SF: Signed Int -> Float
mlir::Value conv_SF(mlir::Value From, mlir::Type F, mlir::Type T,
                    mlir::OpBuilder &Builder) {
  auto Loc = Builder.getUnknownLoc();
  return mlir::arith::SIToFPOp::create(Builder, Loc, T, From);
}

// UF: Unsigned Int -> Float
mlir::Value conv_UF(mlir::Value From, mlir::Type F, mlir::Type T,
                    mlir::OpBuilder &Builder) {
  auto Loc = Builder.getUnknownLoc();
  return mlir::arith::UIToFPOp::create(Builder, Loc, T, From);
}

// FS: Float -> Signed Int
mlir::Value conv_FS(mlir::Value From, mlir::Type F, mlir::Type T,
                    mlir::OpBuilder &Builder) {
  auto Loc = Builder.getUnknownLoc();
  return mlir::arith::FPToSIOp::create(Builder, Loc, T, From);
}

// FU: Float -> Unsigned Int
mlir::Value conv_FU(mlir::Value From, mlir::Type F, mlir::Type T,
                    mlir::OpBuilder &Builder) {
  auto Loc = Builder.getUnknownLoc();
  return mlir::arith::FPToUIOp::create(Builder, Loc, T, From);
}

using ConvHandler = mlir::Value (*)(mlir::Value, mlir::Type, mlir::Type,
                                    mlir::OpBuilder &);

const ConvHandler ConversionTable[3][3] = {
    //  To: Signed    To: Unsigned   To: Float
    {conv_SS, conv_SU, conv_SF}, // From: Signed
    {conv_US, conv_UU, conv_UF}, // From: Unsigned
    {conv_FS, conv_FU, conv_FF}, // From: Float
};
} // namespace

mlir::Type MLIRGen::toMLIRType(QualType T) {
  if (T.isNull() || T.isUnitType()) {
    return Builder.getNoneType();
  }
  if (T.isBooleanType()) {
    return Builder.getI1Type();
  }
  if (T.isIntegerType()) {
    size_t Width = T.getSizeInBytes() * 8;
    return Builder.getIntegerType(Width);
  }
  if (T.isFloatingType()) {
    size_t Width = T.getSizeInBytes() * 8;
    switch (Width) {
    case 32:
      return Builder.getF32Type();
    default:
      return Builder.getF64Type();
    }
  }
  if (T.isPointerType()) {
    mlir::Type Pointee = toMLIRType(T.getBaseType());
    return mlir::MemRefType::get({}, Pointee);
  }
  if (T.isReferenceType()) {
    mlir::Type Referent = toMLIRType(T.getBaseType());
    return mlir::MemRefType::get({}, Referent);
  }
  if (T.isArrayType()) {
    llvm::SmallVector<int64_t> Shape;
    QualType Current = T;
    while (Current.isArrayType()) {
      const ArrayType *AT =
          static_cast<const ArrayType *>(Current.getTypePtr());
      Shape.push_back(static_cast<int64_t>(AT->getArraySize()));
      Current = AT->getElementType();
    }
    mlir::Type Element = toMLIRType(Current);
    return mlir::VectorType::get(Shape, Element);
  }
  return Builder.getNoneType();
}

mlir::MemRefType MLIRGen::toMemRefType(QualType T) {
  if (T.isArrayType()) {
    auto VecTy = llvm::cast<mlir::VectorType>(toMLIRType(T));
    return mlir::MemRefType::get(VecTy.getShape(), VecTy.getElementType());
  }
  return mlir::MemRefType::get({}, toMLIRType(T));
}

const llvm::APFloat MLIRGen::toAPFloat(double D, QualType &Type) {
  if (Type.getSizeInBytes() == 4)
    return llvm::APFloat(static_cast<float>(D));
  else
    return llvm::APFloat(D);
}

bool MLIRGen::isLValue(Expr *E) {
  return E->isVar() || E->getASTNodeKind() == ASTNodeKind::ASTK_ARRAYACCESSEXPR;
}

mlir::Value MLIRGen::getOpMemRef(mlir::Operation *Op) {
  if (auto AllocaOp = mlir::dyn_cast<mlir::memref::AllocaOp>(Op)) {
    return AllocaOp.getMemref();
  } else if (auto AllocOp = mlir::dyn_cast<mlir::memref::AllocOp>(Op)) {
    return AllocOp.getMemref();
  } else {
    llvm_unreachable("Operation not handling is not supported.");
    mlir::Value();
  }
}

mlir::Value MLIRGen::getLValueMemRef(Expr *E) {
  // NameResolver already resolved and cached the Symbol on the node.
  if (auto *VE = llvm::dyn_cast<VarExpr>(E)) {
    Symbol *Sym = VE->getSymbol();
    mlir::Operation *RawOp = static_cast<mlir::Operation *>(Sym->Op);
    return getOpMemRef(RawOp);
  } else if (auto *AE = llvm::dyn_cast<ArrayAccessExpr>(E)) {
    Symbol *Sym = AE->getArrayNameExpr()->getSymbol();
    mlir::Operation *RawOp = static_cast<mlir::Operation *>(Sym->Op);
    return getOpMemRef(RawOp);
  }
  return mlir::Value();
}

mlir::Value MLIRGen::convertValueToType(mlir::Value From, mlir::Type ToType) {
  mlir::Type FromType = From.getType();

  if (FromType == ToType)
    return From;

  NumKind FromKind = getNumKind(FromType);
  NumKind ToKind = getNumKind(ToType);

  return ConversionTable[static_cast<int>(FromKind)][static_cast<int>(ToKind)](
      From, FromType, ToType, Builder);
}

MLIRGen::ReturnState MLIRGen::genStmt(Stmt *S, ReturnState State) {
  if (!S)
    return State;

  // Once a nested region may have returned, execute the following statement
  // only on the path where that return did not happen.  The state is carried
  // as SSA values so no func.return is ever emitted inside an SCF region.
  if (State.Flag) {
    auto Loc = Builder.getUnknownLoc();
    auto True = mlir::arith::ConstantIntOp::create(
        Builder, Loc, Builder.getI1Type(), 1);
    auto Continue = mlir::arith::XOrIOp::create(Builder, Loc, State.Flag,
                                                True.getResult());

    if (!stmtMayReturn(S)) {
      auto IfOp = mlir::scf::IfOp::create(Builder, Loc, Continue,
                                          /*withElseRegion=*/false);
      {
        mlir::OpBuilder::InsertionGuard Guard(Builder);
        Builder.setInsertionPointToStart(&IfOp.getThenRegion().front());
        (void)genStmt(S, ReturnState{});
        mlir::scf::YieldOp::create(Builder, Loc);
      }
      Builder.setInsertionPointAfter(IfOp);
      return State;
    }

    llvm::SmallVector<mlir::Type, 2> ResultTypes;
    ResultTypes.push_back(Builder.getI1Type());
    if (CurrentFunctionHasResult)
      ResultTypes.push_back(CurrentFunctionResultType);

    auto IfOp = mlir::scf::IfOp::create(Builder, Loc, ResultTypes, Continue,
                                        /*withElseRegion=*/true);
    {
      mlir::OpBuilder::InsertionGuard Guard(Builder);
      Builder.setInsertionPointToStart(&IfOp.getThenRegion().front());
      ReturnState ThenState = genStmt(S, ReturnState{});
      mlir::Value ThenFlag =
          ThenState.Flag
              ? ThenState.Flag
              : mlir::arith::ConstantIntOp::create(
                    Builder, Loc, Builder.getI1Type(), 0)
                    .getResult();
      if (CurrentFunctionHasResult) {
        mlir::Value ThenValue = ThenState.Value
                                    ? ThenState.Value
                                    : createDefaultReturnValue();
        mlir::scf::YieldOp::create(Builder, Loc,
                                   mlir::ValueRange{ThenFlag, ThenValue});
      } else {
        mlir::scf::YieldOp::create(Builder, Loc, ThenFlag);
      }
    }
    {
      mlir::OpBuilder::InsertionGuard Guard(Builder);
      Builder.setInsertionPointToStart(&IfOp.getElseRegion().front());
      if (CurrentFunctionHasResult) {
        mlir::Value ElseValue =
            State.Value ? State.Value : createDefaultReturnValue();
        mlir::scf::YieldOp::create(
            Builder, Loc, mlir::ValueRange{State.Flag, ElseValue});
      } else {
        mlir::scf::YieldOp::create(Builder, Loc, State.Flag);
      }
    }
    Builder.setInsertionPointAfter(IfOp);
    ReturnState Result;
    Result.Flag = IfOp.getResult(0);
    if (CurrentFunctionHasResult)
      Result.Value = IfOp.getResult(1);
    return Result;
  }

  switch (S->getASTNodeKind()) {
  case ASTNodeKind::ASTK_FUNCDECL:
    genFuncDecl(static_cast<FuncDecl *>(S));
    return State;
  case ASTNodeKind::ASTK_LETSTMT:
    genLetStmt(static_cast<LetStmt *>(S));
    return State;
  case ASTNodeKind::ASTK_IFSTMT:
    return genIfStmt(static_cast<IfStmt *>(S));
  case ASTNodeKind::ASTK_RETURNSTMT:
    return genReturnStmt(static_cast<ReturnStmt *>(S));
  case ASTNodeKind::ASTK_BLOCKSTMT:
    return genBlockStmt(static_cast<BlockStmt *>(S), State);
  case ASTNodeKind::ASTK_EXPRSTMT:
    genExprStmt(static_cast<ExprStmt *>(S));
    return State;
  case ASTNodeKind::ASTK_FORSTMT:
    genForStmt(static_cast<ForStmt *>(S));
    return State;
  case ASTNodeKind::ASTK_WHILESTMT:
    genWhileStmt(static_cast<WhileStmt *>(S));
    return State;
  default:
    llvm_unreachable("Unhandled statement type");
  }
}

void MLIRGen::genProgram(Program *Node) {
  for (Stmt *S : Node->getStatements()) {
    if (S->getASTNodeKind() == ASTNodeKind::ASTK_FUNCDECL)
      declareFuncDecl(static_cast<FuncDecl *>(S));
  }

  for (Stmt *S : Node->getStatements()) {
    genStmt(S);
  }
}

MLIRGen::ReturnState MLIRGen::genBlockStmt(BlockStmt *Node,
                                           ReturnState State) {
  for (Stmt *S : Node->getStatements())
    State = genStmt(S, State);
  return State;
}

void MLIRGen::genLetStmt(LetStmt *Node) {
  mlir::Location Loc = Builder.getUnknownLoc();
  mlir::Value InitValue;
  Symbol *Sym = Node->getDeclaredVar()->getSymbol();
  QualType VarTy = Sym->Ty;

  static constexpr size_t StackThreshold = 1024; // 1 KB
  size_t SizeInByte = VarTy.getSizeInBytes();

  mlir::Operation *StorageOp = nullptr;
  mlir::Value MemRef;
  if (SizeInByte > StackThreshold) {
    mlir::OpBuilder::InsertionGuard Guard(Builder);
    Builder.setInsertionPointToStart(this->CurrentEntryBlock);
    auto HeapAlloc =
        mlir::memref::AllocOp::create(Builder, Loc, toMemRefType(VarTy));
    StorageOp = HeapAlloc.getOperation();
    MemRef = HeapAlloc.getMemref();
  } else {
    mlir::OpBuilder::InsertionGuard Guard(Builder);
    Builder.setInsertionPointToStart(this->CurrentEntryBlock);
    auto StackAlloc =
        mlir::memref::AllocaOp::create(Builder, Loc, toMemRefType(VarTy));
    StorageOp = StackAlloc.getOperation();
    MemRef = StackAlloc.getMemref();
  }
  Sym->setOp(static_cast<void *>(StorageOp));

  Expr *Init = Node->getInitializer();
  if (!Init)
    return;
  if (Init->getASTNodeKind() == ASTNodeKind::ASTK_ARRAYEXPR) {
    genArrayInit(static_cast<ArrayExpr *>(Init), MemRef, VarTy);
  } else {
    InitValue = visit(Init);
    if (InitValue) {
      mlir::memref::StoreOp::create(Builder, Loc, InitValue, MemRef);
    }
  }
}

void MLIRGen::genArrayInitImpl(ArrayExpr *Node, mlir::Value DestMemRef,
                               llvm::SmallVectorImpl<mlir::Value> &Indices) {
  auto Loc = Builder.getUnknownLoc();
  const auto &Elems = Node->getChildElemExprVec();
  int64_t Count = Node->getTrailingDim()->getValue();

  for (int64_t I = 0; I < Count; ++I) {
    mlir::Value Idx = mlir::arith::ConstantIndexOp::create(Builder, Loc, I);
    Indices.push_back(Idx);

    // [e; N] → Elems.size()==1, I % 1 == 0 always → repeats Elems[0]
    // [e1,e2,..] → Elems.size()==N, I % N == I → picks Elems[I]
    Expr *Elem = Elems[I % Elems.size()];
    if (Elem->getASTNodeKind() == ASTNodeKind::ASTK_ARRAYEXPR) {
      genArrayInitImpl(static_cast<ArrayExpr *>(Elem), DestMemRef, Indices);
    } else if (Elem->isVar()) {
      Symbol *Sym = static_cast<VarExpr *>(Elem)->getSymbol();
      if (Sym && Sym->Ty.isArrayType()) {
        mlir::Operation *RawOp = static_cast<mlir::Operation *>(Sym->Op);
        auto SrcMemRef = getOpMemRef(RawOp);
        auto SrcType = llvm::cast<mlir::MemRefType>(SrcMemRef.getType());
        auto DestType = llvm::cast<mlir::MemRefType>(DestMemRef.getType());

        llvm::SmallVector<mlir::OpFoldResult> Offsets, Sizes, Strides;

        for (mlir::Value Idx : Indices) {
          Offsets.push_back(mlir::OpFoldResult(Idx));
          Sizes.push_back(Builder.getIndexAttr(1));
          Strides.push_back(Builder.getIndexAttr(1));
        }
        for (int64_t Dim : SrcType.getShape()) {
          Offsets.push_back(Builder.getIndexAttr(0));
          Sizes.push_back(Builder.getIndexAttr(Dim));
          Strides.push_back(Builder.getIndexAttr(1));
        }

        auto SubViewTy = mlir::memref::SubViewOp::inferRankReducedResultType(
            SrcType.getShape(), DestType, Offsets, Sizes, Strides);

        mlir::Value SubView =
            mlir::memref::SubViewOp::create(
                Builder, Loc, llvm::cast<mlir::MemRefType>(SubViewTy),
                DestMemRef, Offsets, Sizes, Strides)
                .getResult();

        mlir::memref::CopyOp::create(Builder, Loc, SrcMemRef, SubView);
      } else {
        mlir::Value Val = visit(Elem);
        if (Val)
          mlir::memref::StoreOp::create(Builder, Loc, Val, DestMemRef,
                                        mlir::ValueRange(Indices));
        else
          llvm::errs() << "Error: failed to generate array element at index "
                       << I << "\n";
      }
    } else {
      mlir::Value Val = visit(Elem);
      if (Val)
        mlir::memref::StoreOp::create(Builder, Loc, Val, DestMemRef,
                                      mlir::ValueRange(Indices));
      else
        llvm::errs() << "Error: failed to generate array element at index " << I
                     << "\n";
    }
    Indices.pop_back();
  }
}

Expr *getUniformRepeatChild(ArrayExpr *Node) {
  const auto &Elems = Node->getChildElemExprVec();
  if (Elems.size() != 1)
    return nullptr;

  Expr *Elem = Elems[0];

  if (Elem->getASTNodeKind() == ASTNodeKind::ASTK_ARRAYEXPR)
    return getUniformRepeatChild(static_cast<ArrayExpr *>(Elem));

  if (!Elem->getType().isNull() && Elem->getType().isArrayType())
    return nullptr;

  return Elem;
}

void MLIRGen::genArrayInit(ArrayExpr *Node, mlir::Value DestMemRef,
                           QualType ArrayTy) {
  // FIXME:: When initializing array of size greater than StackThreshold(1KB),
  // linalg.fill will only be used for array with one element all other cases
  // will go to AllocOp.
  // Example: [[1,2,3,4];167], this mlirgen for this will be repeated alloc 167
  // times.
  size_t SizeInByte = Node->getType().getSizeInBytes();
  if (SizeInByte > 1024) {
    if (Expr *Child = getUniformRepeatChild(Node)) {
      mlir::Value FillValue = visit(Child);
      if (FillValue) {
        mlir::linalg::FillOp::create(Builder, Builder.getUnknownLoc(),
                                     mlir::ValueRange{FillValue},
                                     mlir::ValueRange{DestMemRef});
        return;
      }
    }
  }
  llvm::SmallVector<mlir::Value, 4> Indices;
  genArrayInitImpl(Node, DestMemRef, Indices);
}

mlir::Value MLIRGen::visitIntExpr(IntExpr *Node) {
  auto IntOp = mlir::arith::ConstantIntOp::create(
      Builder, Builder.getUnknownLoc(), toMLIRType(Node->getType()),
      Node->getValue());
  return IntOp.getResult();
}

mlir::Value MLIRGen::visitFloatExpr(FloatExpr *Node) {
  const llvm::APFloat FloatValue = toAPFloat(Node->getValue(), Node->getType());
  auto FloatOp = mlir::arith::ConstantFloatOp::create(
      Builder, Builder.getUnknownLoc(),
      llvm::dyn_cast<mlir::FloatType>(toMLIRType(Node->getType())), FloatValue);
  return FloatOp.getResult();
}

mlir::Value MLIRGen::visitRefrExpr(RefrExpr *Node) {
  auto Loc = Builder.getUnknownLoc();
  Expr *Referent = Node->getReferent();

  if (isLValue(Referent)) {
    return getLValueMemRef(Referent);
  }

  mlir::Value Val = visit(Referent);
  auto TempAlloca = mlir::memref::AllocaOp::create(
      Builder, Loc, toMemRefType(Referent->getType()));
  mlir::memref::StoreOp::create(Builder, Loc, Val, TempAlloca.getResult());
  return TempAlloca.getResult();
}

mlir::Value MLIRGen::visitArrayAccessExpr(ArrayAccessExpr *Node) {
  auto Loc = Builder.getUnknownLoc();
  Symbol *Sym = Node->getArrayNameExpr()->getSymbol();
  mlir::Operation *RawPtr = static_cast<mlir::Operation *>(Sym->Op);
  auto memref = getOpMemRef(RawPtr);
  llvm::SmallVector<mlir::Value, 4> IndexValueVec;
  for (Expr *Index : Node->getIndexVector()) {
    mlir::Value IndexValue;
    if (Index->isNum()) {
      IndexValue = visitIntExpr(static_cast<IntExpr *>(Index));
    } else if (Index->isVar()) {
      IndexValue = visitVarExpr(static_cast<VarExpr *>(Index));
    } else
      llvm_unreachable("Index can only be integer or variable.");
    mlir::Value IV = mlir::arith::IndexCastOp::create(
        Builder, Loc, Builder.getIndexType(), IndexValue);
    IndexValueVec.push_back(IV);
  }
  mlir::ValueRange Indices(IndexValueVec);
  mlir::memref::LoadOp loadOp =
      mlir::memref::LoadOp::create(Builder, Loc, memref, Indices);
  return loadOp.getResult();
}

mlir::Value MLIRGen::visitASExpr(ASExpr *Node) {
  mlir::Value FromExpr = visit(Node->getFromExpr());
  QualType ToType = Node->getType();
  return convertValueToType(FromExpr, toMLIRType(ToType));
}

mlir::Value MLIRGen::visitBinExpr(BinExpr *Node) {
  // Visit left and right expressions to get their MLIR values
  mlir::Value LHS = visit(Node->getLHS());
  mlir::Value RHS = visit(Node->getRHS());

  if (!LHS || !RHS) {
    llvm::errs()
        << "Error: Failed to generate operands for binary expression\n";
    return mlir::Value();
  }

  auto Loc = Builder.getUnknownLoc();
  Lex::TokenKind Op = Node->getOp();

  // Get the result type from the typed AST (semantic analysis already
  // determined this)
  QualType ResultTy = Node->getType();

  // Determine if we're working with integers or floats
  bool IsFloat = ResultTy.isFloatingType();
  // bool IsBool = ResultTy.isBooleanType();
  bool IsSigned = ResultTy.isSignedIntegerType();

  // For comparisons, we need to know if the operands are signed or floating
  mlir::Type LHSTy = LHS.getType();
  bool OpIsFloat = LHSTy.isFloat();
  bool OpIsSigned = Node->getLHS()->getType().isSignedIntegerType();

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
    } else if (IsSigned) {
      return mlir::arith::DivSIOp::create(Builder, Loc, LHS, RHS);
    } else {
      return mlir::arith::DivUIOp::create(Builder, Loc, LHS, RHS);
    }

  // ========================================================================
  // Comparison Operators (return i1/bool)
  // ========================================================================
  case Lex::TokenKind::OP_EQUALEQUAL:
    if (OpIsFloat) {
      return mlir::arith::CmpFOp::create(
          Builder, Loc,
          mlir::arith::CmpFPredicate::OEQ, // Ordered equal
          LHS, RHS);
    } else {
      return mlir::arith::CmpIOp::create(
          Builder, Loc, mlir::arith::CmpIPredicate::eq, LHS, RHS);
    }

  case Lex::TokenKind::OP_BANGEQUAL:
    if (OpIsFloat) {
      return mlir::arith::CmpFOp::create(
          Builder, Loc,
          mlir::arith::CmpFPredicate::ONE, // Ordered not equal
          LHS, RHS);
    } else {
      return mlir::arith::CmpIOp::create(
          Builder, Loc, mlir::arith::CmpIPredicate::ne, LHS, RHS);
    }

  case Lex::TokenKind::OP_LESS:
    if (OpIsFloat) {
      return mlir::arith::CmpFOp::create(
          Builder, Loc,
          mlir::arith::CmpFPredicate::OLT, // Ordered less than
          LHS, RHS);
    } else {
      return mlir::arith::CmpIOp::create(Builder, Loc,
                                         OpIsSigned
                                             ? mlir::arith::CmpIPredicate::slt
                                             : mlir::arith::CmpIPredicate::ult,
                                         LHS, RHS);
    }

  case Lex::TokenKind::OP_LESSEQUAL:
    if (OpIsFloat) {
      return mlir::arith::CmpFOp::create(
          Builder, Loc,
          mlir::arith::CmpFPredicate::OLE, // Ordered less or equal
          LHS, RHS);
    } else {
      return mlir::arith::CmpIOp::create(Builder, Loc,
                                         OpIsSigned
                                             ? mlir::arith::CmpIPredicate::sle
                                             : mlir::arith::CmpIPredicate::ule,
                                         LHS, RHS);
    }

  case Lex::TokenKind::OP_GREATER:
    if (OpIsFloat) {
      return mlir::arith::CmpFOp::create(
          Builder, Loc,
          mlir::arith::CmpFPredicate::OGT, // Ordered greater than
          LHS, RHS);
    } else {
      return mlir::arith::CmpIOp::create(Builder, Loc,
                                         OpIsSigned
                                             ? mlir::arith::CmpIPredicate::sgt
                                             : mlir::arith::CmpIPredicate::ugt,
                                         LHS, RHS);
    }

  case Lex::TokenKind::OP_GREATEREQUAL:
    if (OpIsFloat) {
      return mlir::arith::CmpFOp::create(
          Builder, Loc,
          mlir::arith::CmpFPredicate::OGE, // Ordered greater or equal
          LHS, RHS);
    } else {
      return mlir::arith::CmpIOp::create(Builder, Loc,
                                         OpIsSigned
                                             ? mlir::arith::CmpIPredicate::sge
                                             : mlir::arith::CmpIPredicate::uge,
                                         LHS, RHS);
    }

  // ========================================================================
  // Logical Operators (boolean operations)
  // ========================================================================
  case Lex::TokenKind::OP_AMPAMP: // &&
                                  // LHS && RHS
    return mlir::arith::AndIOp::create(Builder, Loc, LHS, RHS);

  case Lex::TokenKind::OP_PIPEPIPE: // ||
                                    // LHS || RHS
    return mlir::arith::OrIOp::create(Builder, Loc, LHS, RHS);

  default:
    llvm::errs() << "Error: Unhandled binary operator: "
                 << Lex::getTokenName(Op) << "\n";
    return mlir::Value();
  }
}

mlir::Value MLIRGen::visitUnaryExpr(UnaryExpr *Node) {
  auto Loc = Builder.getUnknownLoc();
  mlir::Value Operand = visit(Node->getOperand());
  if (!Operand)
    return mlir::Value();

  switch (Node->getOp()) {
  case Lex::TokenKind::OP_MINUS:
    if (Node->getType().isFloatingType())
      return mlir::arith::NegFOp::create(Builder, Loc, Operand);
    if (Node->getType().isIntegerType()) {
      auto Zero = mlir::arith::ConstantIntOp::create(
          Builder, Loc, Operand.getType(), 0);
      return mlir::arith::SubIOp::create(Builder, Loc, Zero, Operand);
    }
    break;
  case Lex::TokenKind::OP_BANG:
    if (Node->getType().isBooleanType()) {
      auto True = mlir::arith::ConstantIntOp::create(
          Builder, Loc, Operand.getType(), 1);
      return mlir::arith::XOrIOp::create(Builder, Loc, Operand, True);
    }
    break;
  default:
    break;
  }

  llvm::errs() << "Error: Unhandled unary operator: "
               << Lex::getTokenName(Node->getOp()) << "\n";
  return mlir::Value();
}

mlir::Value MLIRGen::visitBoolExpr(BoolExpr *Node) {
  auto BoolOp = mlir::arith::ConstantIntOp::create(
      Builder, Builder.getUnknownLoc(), toMLIRType(Node->getType()),
      Node->getValue());
  return BoolOp;
}

mlir::Value MLIRGen::visitStringExpr(StringExpr *Node) {
  return mlir::Value();
}

mlir::Value MLIRGen::visitVarExpr(VarExpr *Node) {
  Symbol *Sym = Node->getSymbol();
  mlir::Operation *RawPtr = static_cast<mlir::Operation *>(Sym->Op);
  auto allocaOp = llvm::dyn_cast<mlir::memref::AllocaOp>(RawPtr);

  mlir::memref::LoadOp loadOp = mlir::memref::LoadOp::create(
      Builder, Builder.getUnknownLoc(), allocaOp.getMemref());
  return loadOp.getResult();
}

mlir::Value MLIRGen::visitFunCall(FunCall *Node) {
  auto Loc = Builder.getUnknownLoc();

  const std::string &FuncName = Node->getFuncName()->getName();

  Symbol *FuncSym = Node->getFuncName()->getSymbol();
  if (!FuncSym) {
    llvm::errs() << "Error: Function '" << FuncName
                 << "' not found in symbol table\n";
    return mlir::Value();
  }

  mlir::Operation *RawPtr = static_cast<mlir::Operation *>(FuncSym->Op);
  auto funcOp = llvm::dyn_cast_if_present<mlir::func::FuncOp>(RawPtr);
  if (!funcOp) {
    llvm::errs() << "Error: Function '" << FuncName
                 << "' has no generated definition\n";
    return mlir::Value();
  }

  llvm::ArrayRef<mlir::Type> ResultTypes = funcOp.getResultTypes();

  QualType FuncTy = FuncSym->Ty;
  if (!FuncTy.isFunctionType()) {
    llvm::errs() << "Error: Symbol '" << FuncName << "' is not a function\n";
    return mlir::Value();
  }

  llvm::SmallVector<mlir::Value, 4> Args;
  for (Expr *ArgExpr : Node->getParams()) {
    mlir::Value ArgValue = visit(ArgExpr);
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

namespace {

mlir::func::FuncOp getRuntimeFunction(mlir::ModuleOp Module,
                                      llvm::StringRef Name,
                                      mlir::FunctionType Type) {
  if (auto Function = Module.lookupSymbol<mlir::func::FuncOp>(Name))
    return Function;

  mlir::OpBuilder ModuleBuilder(Module.getBodyRegion());
  auto Function =
      mlir::func::FuncOp::create(ModuleBuilder, Module.getLoc(), Name, Type);
  Function.setPrivate();
  return Function;
}

} // namespace

void MLIRGen::emitPrintLiteral(std::string_view Text) {
  if (Text.empty())
    return;

  auto Loc = Builder.getUnknownLoc();
  llvm::StringRef Contents(Text.data(), Text.size());

  auto Entry = PrintStringGlobals.find(Contents);
  if (Entry == PrintStringGlobals.end()) {
    std::string Name =
        "trsc_print_str." + std::to_string(PrintStringGlobals.size());
    auto ArrayType =
        mlir::LLVM::LLVMArrayType::get(Builder.getI8Type(), Text.size());

    mlir::OpBuilder ModuleBuilder(Module.getBodyRegion());
    mlir::LLVM::GlobalOp::create(ModuleBuilder, Module.getLoc(), ArrayType,
                                 /*isConstant=*/true,
                                 mlir::LLVM::Linkage::Internal, Name,
                                 Builder.getStringAttr(Contents));
    Entry = PrintStringGlobals.insert({Contents, Name}).first;
  }

  auto PointerType = mlir::LLVM::LLVMPointerType::get(&MLIRCtx);
  mlir::Value Pointer =
      mlir::LLVM::AddressOfOp::create(Builder, Loc, PointerType, Entry->second);
  mlir::Value Length = mlir::LLVM::ConstantOp::create(
      Builder, Loc, Builder.getI64Type(),
      Builder.getI64IntegerAttr(static_cast<int64_t>(Text.size())));

  mlir::FunctionType Type =
      Builder.getFunctionType({Pointer.getType(), Length.getType()}, {});
  mlir::Value Arguments[] = {Pointer, Length};
  mlir::func::CallOp::create(Builder, Loc,
                             getRuntimeFunction(Module, "trsc_print_str", Type),
                             Arguments);
}

void MLIRGen::emitPrintValue(Expr *Argument) {
  auto Loc = Builder.getUnknownLoc();

  if (auto *String = llvm::dyn_cast<StringExpr>(Argument)) {
    emitPrintLiteral(String->getValue());
    return;
  }

  mlir::Value Value = visit(Argument);
  if (!Value)
    return;

  auto EmitCall = [&](llvm::StringRef Name, mlir::Value Operand) {
    mlir::FunctionType Type = Builder.getFunctionType({Operand.getType()}, {});
    mlir::Value Arguments[] = {Operand};
    mlir::func::CallOp::create(
        Builder, Loc, getRuntimeFunction(Module, Name, Type), Arguments);
  };

  QualType Type = Argument->getType();
  while (Type.isReferenceType()) {
    auto MemRef = llvm::dyn_cast<mlir::MemRefType>(Value.getType());
    if (!MemRef || MemRef.getRank() != 0)
      return;
    Value = mlir::memref::LoadOp::create(Builder, Loc, Value).getResult();
    Type = Type.getBaseType();
  }

  if (Type.isBooleanType()) {
    mlir::Value Normalized =
        mlir::arith::ExtUIOp::create(Builder, Loc, Builder.getI32Type(), Value);
    EmitCall("trsc_print_bool", Normalized);
    return;
  }

  if (Type.isIntegerType()) {
    unsigned Width = static_cast<unsigned>(Type.getSizeInBytes() * 8);
    bool IsSigned = Type.isSignedIntegerType();
    mlir::Type TargetType = Builder.getIntegerType(64);
    mlir::Value Normalized = Value;
    if (Width < 64) {
      if (IsSigned) {
        Normalized =
            mlir::arith::ExtSIOp::create(Builder, Loc, TargetType, Value);
      } else {
        Normalized =
            mlir::arith::ExtUIOp::create(Builder, Loc, TargetType, Value);
      }
    }
    EmitCall(IsSigned ? "trsc_print_i64" : "trsc_print_u64", Normalized);
    return;
  }

  if (Type.isFloatingType()) {
    EmitCall(Type.getSizeInBytes() == 4 ? "trsc_print_f32" : "trsc_print_f64",
             Value);
  }
}

mlir::Value MLIRGen::visitMacroCall(MacroCall *Node) {
  auto Loc = Builder.getUnknownLoc();

  std::string_view Format = Node->getFormatString();
  std::string Literal;
  std::size_t ParamIndex = 0;

  for (std::size_t I = 0; I < Format.size(); ++I) {
    if (Format[I] == '{') {
      if (I + 1 < Format.size() && Format[I + 1] == '{') {
        Literal.push_back('{');
        ++I;
        continue;
      }
      std::size_t Close = Format.find('}', I + 1);
      if (Close == std::string_view::npos)
        break;
      emitPrintLiteral(Literal);
      Literal.clear();
      if (ParamIndex < Node->getParams().size())
        emitPrintValue(Node->getParams()[ParamIndex++]);
      I = Close;
      continue;
    }
    if (Format[I] == '}' && I + 1 < Format.size() && Format[I + 1] == '}') {
      Literal.push_back('}');
      ++I;
      continue;
    }
    Literal.push_back(Format[I]);
  }
  emitPrintLiteral(Literal);

  auto Newline = getRuntimeFunction(Module, "trsc_print_newline",
                                    Builder.getFunctionType({}, {}));
  mlir::func::CallOp::create(Builder, Loc, Newline, mlir::ValueRange());
  return mlir::Value();
}

void MLIRGen::genParams(ArrayRef<FuncDecl::Param> Params) {
  auto Loc = Builder.getUnknownLoc();

  for (size_t i = 0; i < Params.size(); ++i) {
    const auto &Param = Params[i];

    Symbol *ParamSym = Param.ParamName->getSymbol();

    if (!ParamSym) {
      llvm::errs() << "Error: Parameter '" << Param.ParamName->getName()
                   << "' not found in symbol table\n";
      continue;
    }

    QualType ParamType = ParamSym->Ty;

    auto ParamAlloca =
        mlir::memref::AllocaOp::create(Builder, Loc, toMemRefType(ParamType));

    // Get the corresponding block argument (function parameter value)
    mlir::Value BlockArg = CurrentEntryBlock->getArgument(i);

    // Store the block argument into the alloca
    mlir::memref::StoreOp::create(Builder, Loc, BlockArg,
                                  ParamAlloca.getResult());

    // Update symbol table to point to this alloca
    ParamSym->setOp(static_cast<void *>(ParamAlloca.getOperation()));
  }
}

mlir::Operation *MLIRGen::declareFuncDecl(FuncDecl *Node) {
  Symbol *Sym = Node->getFuncName()->getSymbol();
  if (!Sym)
    return nullptr;

  QualType FuncTy = Sym->Ty;
  if (!FuncTy.isFunctionType())
    return nullptr;

  if (Sym->Op)
    return static_cast<mlir::Operation *>(Sym->Op);

  std::vector<mlir::Type> ArgTypes;
  for (const auto &ParamTy : FuncTy.getParamsType()) {
    ArgTypes.push_back(toMLIRType(ParamTy));
  }

  QualType RetT = FuncTy.getReturnType();
  std::vector<mlir::Type> ResultTypes;
  const bool IsEntryPoint = Node->getFuncName()->getName() == "main" &&
                            Node->getParams().empty();
  if (!RetT.isUnitType()) {
    ResultTypes.push_back(toMLIRType(RetT));
  } else if (IsEntryPoint) {
    // The native entry point follows the C ABI even when the source-level
    // function has the language's unit return type.  A void `main` leaves
    // the process exit status undefined after linking.
    ResultTypes.push_back(Builder.getI32Type());
  }

  auto FuncType = Builder.getFunctionType(ArgTypes, ResultTypes);

  auto funcOp =
      mlir::func::FuncOp::create(Builder, Builder.getUnknownLoc(),
                                 Node->getFuncName()->getName(), FuncType);
  Sym->setOp(static_cast<void *>(funcOp.getOperation()));
  return funcOp.getOperation();
}

void MLIRGen::genFuncDecl(FuncDecl *Node) {
  mlir::Operation *RawFuncOp = declareFuncDecl(Node);
  if (!RawFuncOp)
    return;
  auto funcOp = llvm::cast<mlir::func::FuncOp>(RawFuncOp);
  llvm::ArrayRef<mlir::Type> ResultTypes = funcOp.getResultTypes();

  CurrentFunctionHasResult = !ResultTypes.empty();
  CurrentFunctionResultType =
      CurrentFunctionHasResult ? ResultTypes.front() : mlir::Type();

  auto *EntryBlock = funcOp.addEntryBlock();
  this->CurrentEntryBlock = EntryBlock;

  mlir::OpBuilder::InsertionGuard Guard(Builder);
  Builder.setInsertionPointToStart(CurrentEntryBlock);

  if (!Node->getParams().empty()) {
    genParams(Node->getParams());
  }

  if (Node->getBody()) {
    ReturnState State = genStmt(Node->getBody(), ReturnState{});
    Builder.setInsertionPointToEnd(EntryBlock);

    if (State.Flag) {
      if (CurrentFunctionHasResult) {
        mlir::Value ReturnValue =
            State.Value ? State.Value : createDefaultReturnValue();
        mlir::func::ReturnOp::create(Builder, Builder.getUnknownLoc(),
                                     ReturnValue);
      } else {
        mlir::func::ReturnOp::create(Builder, Builder.getUnknownLoc());
      }
    }
  }

  const bool IsUnitEntryPoint =
      Node->getFuncName()->getName() == "main" &&
      Node->getParams().empty() && Node->getReturnType() == nullptr;

  if (ResultTypes.empty()) {
    if (EntryBlock->empty() ||
        !EntryBlock->back().hasTrait<mlir::OpTrait::IsTerminator>()) {
      mlir::func::ReturnOp::create(Builder, Builder.getUnknownLoc());
    }
  } else if (IsUnitEntryPoint &&
             (EntryBlock->empty() ||
              !EntryBlock->back().hasTrait<mlir::OpTrait::IsTerminator>())) {
    auto Zero = mlir::arith::ConstantIntOp::create(
        Builder, Builder.getUnknownLoc(), Builder.getI32Type(), 0);
    mlir::func::ReturnOp::create(Builder, Builder.getUnknownLoc(),
                                 Zero.getResult());
  }

  this->CurrentEntryBlock = nullptr;
  CurrentFunctionResultType = mlir::Type();
  CurrentFunctionHasResult = false;
}

MLIRGen::ReturnState MLIRGen::genReturnStmt(ReturnStmt *Node) {
  auto Loc = Builder.getUnknownLoc();
  ReturnState State;

  auto True = mlir::arith::ConstantIntOp::create(Builder, Loc,
                                                 Builder.getI1Type(), 1);
  State.Flag = True.getResult();

  // No return value (void return)
  if (!Node->getReturnValue()) {
    if (CurrentFunctionHasResult)
      State.Value = createDefaultReturnValue();
    return State;
  }

  // Has return value
  mlir::Value RetValue = visit(Node->getReturnValue());

  if (!RetValue) {
    llvm::errs() << "Error: Failed to generate return value expression\n";
    if (CurrentFunctionHasResult)
      State.Value = createDefaultReturnValue();
    return State;
  }

  State.Value = RetValue;
  return State;
}

bool MLIRGen::stmtMayReturn(Stmt *S) const {
  if (!S)
    return false;

  switch (S->getASTNodeKind()) {
  case ASTNodeKind::ASTK_RETURNSTMT:
    return true;
  case ASTNodeKind::ASTK_BLOCKSTMT:
    for (Stmt *Child : static_cast<BlockStmt *>(S)->getStatements()) {
      if (stmtMayReturn(Child))
        return true;
    }
    return false;
  case ASTNodeKind::ASTK_IFSTMT: {
    auto *If = static_cast<IfStmt *>(S);
    return stmtMayReturn(If->getThenBranch()) ||
           stmtMayReturn(If->getElseBranch());
  }
  default:
    return false;
  }
}

mlir::Value MLIRGen::createDefaultReturnValue() {
  if (!CurrentFunctionHasResult)
    return mlir::Value();

  auto Loc = Builder.getUnknownLoc();
  mlir::Type Type = CurrentFunctionResultType;
  if (Type.isIndex())
    return mlir::arith::ConstantIndexOp::create(Builder, Loc, 0).getResult();
  if (Type.isInteger())
    return mlir::arith::ConstantIntOp::create(Builder, Loc, Type, 0)
        .getResult();
  if (auto FloatType = llvm::dyn_cast<mlir::FloatType>(Type)) {
    llvm::APFloat Zero = FloatType.getWidth() == 32
                             ? llvm::APFloat(0.0f)
                             : llvm::APFloat(0.0);
    return mlir::arith::ConstantFloatOp::create(Builder, Loc, FloatType, Zero)
        .getResult();
  }

  llvm::errs() << "Error: Cannot create default return value for type " << Type
               << "\n";
  return mlir::Value();
}

MLIRGen::ReturnState MLIRGen::genIfStmt(IfStmt *Node) {
  auto Loc = Builder.getUnknownLoc();

  // Generate condition (must be i1 type)
  mlir::Value Condition = visit(Node->getCondition());

  if (!Condition) {
    llvm::errs() << "Error: Failed to generate condition for if statement\n";
    return ReturnState{};
  }

  // Verify condition is i1 type
  if (!Condition.getType().isInteger(1)) {
    llvm::errs() << "Error: If condition must be of type i1 (bool), got: "
                 << Condition.getType() << "\n";
    return ReturnState{};
  }

  const bool HasReturn = stmtMayReturn(Node->getThenBranch()) ||
                         stmtMayReturn(Node->getElseBranch());

  if (!HasReturn) {
    auto IfOp = mlir::scf::IfOp::create(
        Builder, Loc, Condition,
        /*withElseRegion=*/Node->getElseBranch() != nullptr);

    {
      mlir::OpBuilder::InsertionGuard Guard(Builder);
      Builder.setInsertionPointToStart(&IfOp.getThenRegion().front());
      (void)genStmt(Node->getThenBranch(), ReturnState{});
      mlir::Block *ThenBlock = &IfOp.getThenRegion().front();
      if (ThenBlock->empty() ||
          !ThenBlock->back().hasTrait<mlir::OpTrait::IsTerminator>())
        mlir::scf::YieldOp::create(Builder, Loc);
    }

    if (Node->getElseBranch()) {
      mlir::OpBuilder::InsertionGuard Guard(Builder);
      Builder.setInsertionPointToStart(&IfOp.getElseRegion().front());
      (void)genStmt(Node->getElseBranch(), ReturnState{});
      mlir::Block *ElseBlock = &IfOp.getElseRegion().front();
      if (ElseBlock->empty() ||
          !ElseBlock->back().hasTrait<mlir::OpTrait::IsTerminator>())
        mlir::scf::YieldOp::create(Builder, Loc);
    }
    Builder.setInsertionPointAfter(IfOp);
    return ReturnState{};
  }

  llvm::SmallVector<mlir::Type, 2> ResultTypes;
  ResultTypes.push_back(Builder.getI1Type());
  if (CurrentFunctionHasResult)
    ResultTypes.push_back(CurrentFunctionResultType);

  // A result-bearing scf.if carries both the fact that a branch returned and
  // the value to return.  Both regions yield, so the enclosing function can
  // emit the only func.return after all structured control flow is complete.
  auto IfOp = mlir::scf::IfOp::create(Builder, Loc, ResultTypes, Condition,
                                      /*withElseRegion=*/true);

  auto YieldState = [&](ReturnState State) {
    mlir::Value Flag =
        State.Flag ? State.Flag
                   : mlir::arith::ConstantIntOp::create(
                         Builder, Loc, Builder.getI1Type(), 0)
                         .getResult();
    if (CurrentFunctionHasResult) {
      mlir::Value Value =
          State.Value ? State.Value : createDefaultReturnValue();
      mlir::scf::YieldOp::create(Builder, Loc,
                                 mlir::ValueRange{Flag, Value});
    } else {
      mlir::scf::YieldOp::create(Builder, Loc, Flag);
    }
  };

  // Generate "then" region
  {
    mlir::OpBuilder::InsertionGuard Guard(Builder);
    Builder.setInsertionPointToStart(&IfOp.getThenRegion().front());
    YieldState(genStmt(Node->getThenBranch(), ReturnState{}));
  }

  // Generate "else" region.  A missing else is the non-returning path.
  if (Node->getElseBranch()) {
    mlir::OpBuilder::InsertionGuard Guard(Builder);
    Builder.setInsertionPointToStart(&IfOp.getElseRegion().front());
    YieldState(genStmt(Node->getElseBranch(), ReturnState{}));
  } else {
    mlir::OpBuilder::InsertionGuard Guard(Builder);
    Builder.setInsertionPointToStart(&IfOp.getElseRegion().front());
    YieldState(ReturnState{});
  }

  Builder.setInsertionPointAfter(IfOp);
  ReturnState State;
  State.Flag = IfOp.getResult(0);
  if (CurrentFunctionHasResult)
    State.Value = IfOp.getResult(1);
  return State;
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

  if (Node->getRHS()->getASTNodeKind() == ASTNodeKind::ASTK_ARRAYEXPR) {
    genArrayInit(static_cast<ArrayExpr *>(Node->getRHS()), LHSMemRef,
                 Node->getRHS()->getType());
    return;
  }
  if (auto *AAE = llvm::dyn_cast<ArrayAccessExpr>(Node->getLHS())) {
    mlir::Value BaseMemRef = getLValueMemRef(Node->getLHS());
    llvm::SmallVector<mlir::Value, 4> IndexValueVec;
    for (Expr *Index : AAE->getIndexVector()) {
      mlir::Value IndexValue;
      if (Index->isNum()) {
        IndexValue = visitIntExpr(static_cast<IntExpr *>(Index));
      } else if (Index->isVar()) {
        IndexValue = visitVarExpr(static_cast<VarExpr *>(Index));
      } else
        llvm_unreachable("Index can only be integer or variable.");
      mlir::Value IV = mlir::arith::IndexCastOp::create(
          Builder, Loc, Builder.getIndexType(), IndexValue);
      IndexValueVec.push_back(IV);
    }
    mlir::Value ValueToStore;
    Lex::TokenKind Op = Node->getOp();
    if (Op == Lex::TokenKind::OP_EQUAL) {
      ValueToStore = visit(Node->getRHS());
    } else {
      mlir::Value Current = mlir::memref::LoadOp::create(
          Builder, Loc, BaseMemRef, mlir::ValueRange(IndexValueVec));
      mlir::Value RHS = visit(Node->getRHS());
      bool IsFloat = Node->getType().isFloatingType();
      if (Op == Lex::TokenKind::OP_PLUSEQUAL) {
        ValueToStore =
            IsFloat ? mlir::arith::AddFOp::create(Builder, Loc, Current, RHS)
                          .getResult()
                    : mlir::arith::AddIOp::create(Builder, Loc, Current, RHS)
                          .getResult();
      } else if (Op == Lex::TokenKind::OP_MINUSEQUAL) {
        ValueToStore =
            IsFloat ? mlir::arith::SubFOp::create(Builder, Loc, Current, RHS)
                          .getResult()
                    : mlir::arith::SubIOp::create(Builder, Loc, Current, RHS)
                          .getResult();
      }
    }

    if (ValueToStore)
      mlir::memref::StoreOp::create(Builder, Loc, ValueToStore, BaseMemRef,
                                    mlir::ValueRange(IndexValueVec));
    return;
  }

  mlir::Value ValueToStore;
  Lex::TokenKind Op = Node->getOp();

  if (Op == Lex::TokenKind::OP_EQUAL) {
    ValueToStore = visit(Node->getRHS());
  } else {
    // Compound: a += b or a -= b
    mlir::Value CurrentValue =
        mlir::memref::LoadOp::create(Builder, Loc, LHSMemRef);
    // Evaluate RHS
    mlir::Value RHSValue = visit(Node->getRHS());
    // Perform operation
    QualType ResultTy = Node->getType();
    bool IsFloat = ResultTy.isFloatingType();

    if (Op == Lex::TokenKind::OP_PLUSEQUAL) {
      ValueToStore = IsFloat
                         ? mlir::arith::AddFOp::create(Builder, Loc,
                                                       CurrentValue, RHSValue)
                               .getResult()
                         : mlir::arith::AddIOp::create(Builder, Loc,
                                                       CurrentValue, RHSValue)
                               .getResult();

    } else if (Op == Lex::TokenKind::OP_MINUSEQUAL) {
      ValueToStore = IsFloat
                         ? mlir::arith::SubFOp::create(Builder, Loc,
                                                       CurrentValue, RHSValue)
                               .getResult()
                         : mlir::arith::SubIOp::create(Builder, Loc,
                                                       CurrentValue, RHSValue)
                               .getResult();
    }
  }

  // 3. Store the result
  mlir::memref::StoreOp::create(Builder, Loc, ValueToStore, LHSMemRef);
}

void MLIRGen::genExprStmt(ExprStmt *Node) {
  Expr *E = Node->getExpression();

  if (!E)
    return;

  if (auto *BE = llvm::dyn_cast<BinExpr>(E)) {
    Lex::TokenKind Op = BE->getOp();

    if (Op == Lex::TokenKind::OP_EQUAL || Op == Lex::TokenKind::OP_PLUSEQUAL ||
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
  VarExpr *Init = Node->getInit();
  if (Init) {
    Symbol *Sym = Init->getSymbol();
    auto InitTy = toMemRefType(Sym->Ty);
    auto AllocaOp = mlir::memref::AllocaOp::create(Builder, loc, InitTy);
    Sym->setOp(static_cast<void *>(AllocaOp));
  }
  // 1. Generate loop bounds and step
  mlir::Value lbValue = visit(Node->getRange()->getStart());
  mlir::Value ubValue = visit(Node->getRange()->getEnd());
  mlir::Value lb = mlir::arith::IndexCastOp::create(
      Builder, loc, Builder.getIndexType(), lbValue);
  mlir::Value ub = mlir::arith::IndexCastOp::create(
      Builder, loc, Builder.getIndexType(), ubValue);
  if (Node->getRange()->isInclusive()) {
    mlir::Value One =
        mlir::arith::ConstantIndexOp::create(Builder, loc, 1);
    ub = mlir::arith::AddIOp::create(Builder, loc, ub, One).getResult();
  }
  mlir::Value step = mlir::arith::ConstantIndexOp::create(Builder, loc, 1);

  auto forOp = mlir::scf::ForOp::create(Builder, loc, lb, ub, step);

  mlir::OpBuilder::InsertionGuard guard(Builder);
  Builder.setInsertionPointToStart(forOp.getBody());

  if (Init) {
    Symbol *Sym = Init->getSymbol();
    mlir::Value iv = forOp.getInductionVar();
    mlir::Type elemTy = toMLIRType(Sym->Ty);
    mlir::Value ivCast =
        mlir::arith::IndexCastOp::create(Builder, loc, elemTy, iv);
    mlir::Operation *RawOp = static_cast<mlir::Operation *>(Sym->Op);
    mlir::Value memref = getOpMemRef(RawOp);
    mlir::memref::StoreOp::create(Builder, loc, ivCast, memref);
  }

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
