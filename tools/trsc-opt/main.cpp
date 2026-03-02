#include "mlir/IR/Dialect.h"
#include "mlir/IR/MLIRContext.h"
#include "mlir/Parser/Parser.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Pass/PassManager.h"
#include "mlir/Support/FileUtilities.h"
#include "mlir/Tools/mlir-opt/MlirOptMain.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/ToolOutputFile.h"

// TRSC includes
#include "trsc/MLIR/TrscDialect.h"
#include "trsc/MLIR/TrscPasses.h"

// Standard MLIR dialects that we need
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/ControlFlow/IR/ControlFlowOps.h"

int main(int argc, char **argv) {
  // Register MLIR command line options
  mlir::registerMLIRContextCLOptions();
  mlir::registerPassManagerCLOptions();
  mlir::registerAsmPrinterCLOptions();

  // Register only TRSC passes (not all MLIR passes)
  // trsc::registerTrscPasses();

  // Create dialect registry and register only the dialects we need
  mlir::DialectRegistry registry;
  registry.insert<trsc::TrscDialect>();
  registry.insert<mlir::func::FuncDialect>();
  registry.insert<mlir::arith::ArithDialect>();
  registry.insert<mlir::cf::ControlFlowDialect>();

  return mlir::asMainReturnCode(
      mlir::MlirOptMain(argc, argv, "TRSC optimizer driver\n", registry));
}
