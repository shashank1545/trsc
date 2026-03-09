
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/BuiltinTypeInterfaces.h"
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

  mlir::memref::AllocaOp VarAlloca;
  {
    mlir::OpBuilder::InsertionGuard Guard(Builder);
    Builder.setInsertionPointToStart(this->CurrentEntryBlock);
    VarAlloca = mlir::memref::AllocaOp::create(Builder, 
        Builder.getUnknownLoc(), 
        ToMemRefType(VarTy));  
  }
  Sym->setAlloca(VarAlloca);
void MLIRGen::genFuncDecl(FuncDecl *Node) {
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

  auto FuncOp = mlir::func::FuncOp::create(
      Builder,
      Builder.getUnknownLoc(),
      Node->getFuncName()->getName(),
      FuncType
      );

  auto *EntryBlock = FuncOp.addEntryBlock();
  this->CurrentEntryBlock = EntryBlock;

  mlir::OpBuilder::InsertionGuard Guard(Builder);
  Builder.setInsertionPointToStart(CurrentEntryBlock);

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
