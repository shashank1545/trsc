
#include "mlir/IR/Types.h"
#include "trsc/AST/AST.h"
#include "trsc/AST/ASTVisitor.h"
#include "trsc/AST/QualType.h"
#include "trsc/AST/ASTContext.h"
#include "trsc/MLIR/TrscMLIRGen.h"
#include "trsc/Sema/SymbolTable.h"

#include "mlir/IR/Builders.h"
#include "mlir/IR/MLIRContext.h"
#include "mlir/IR/OwningOpRef.h"
#include "mlir/IR/Verifier.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include <vector>

using namespace trsc; 
using namespace mlir;

MLIRGen::MLIRGen(mlir::MLIRContext &MLIRCtx, trsc::ASTContext &ASTCtx, 
    trsc::SymbolTable &ST) : MLIRCtx(MLIRCtx), ASTCtx(ASTCtx), ST(ST), 
  Builder(&MLIRCtx) {
    MLIRCtx.loadDialect<mlir::func::FuncDialect>();
  }

mlir::Type MLIRGen::convertType(QualType T) {
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

void MLIRGen::visitProgram(Program *Node) {
  ASTVisitor<MLIRGen>::visitProgram(Node);
}

void MLIRGen::visitBlockStmt(BlockStmt *Node) {
  for (const auto &Stmt : Node->getStatements()) {
    visit(Stmt.get());
  }
}

void MLIRGen::visitFuncDecl(FuncDecl *Node) {
  Symbol* Sym = ST.lookupSymbol(Node->getFuncName()->getName());
  if (!Sym) return;

  QualType FuncTy = Sym->Ty;
  if (!FuncTy.isFunctionType()) return;

  std::vector<mlir::Type> ArgTypes;
  for (const auto& ParamTy : FuncTy.getParamsType()) {
    ArgTypes.push_back(convertType(ParamTy));
  }
  
  QualType RetT = FuncTy.getReturnType();
  std::vector<mlir::Type> ResultTypes;
  if (!RetT.isUnitType() && !RetT.isNull()) {
    ResultTypes.push_back(convertType(RetT));
  }

  auto FuncType = Builder.getFunctionType(ArgTypes, ResultTypes);

  auto FuncOp = Builder.create<mlir::func::FuncOp>(
      Builder.getUnknownLoc(),
      Node->getFuncName()->getName(),
      FuncType
      );

  auto *EntryBlock = FuncOp.addEntryBlock();

  mlir::OpBuilder::InsertionGuard Guard(Builder);
  Builder.setInsertionPointToStart(EntryBlock);

  if(Node->getBody()) {
    visit(Node->getBody());
  }

  if (ResultTypes.empty()) {
    if (EntryBlock->empty() || 
        !EntryBlock->back().hasTrait<mlir::OpTrait::IsTerminator>()) {
      Builder.create<mlir::func::ReturnOp>(Builder.getUnknownLoc());
    }
  }
}

mlir::OwningOpRef<mlir::ModuleOp> MLIRGen::generate(trsc::Program &Prog) {
  Module = mlir::ModuleOp::create(mlir::UnknownLoc::get(&MLIRCtx));
  Builder.setInsertionPointToEnd(Module.getBody());
  
  visitProgram(&Prog);

  if (failed(mlir::verify(Module))) {
    Module.emitError("Module verification failed.");
    return nullptr;
  }

  return mlir::OwningOpRef<mlir::ModuleOp>(Module);
}
