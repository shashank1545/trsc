#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/Dialect/ControlFlow/IR/ControlFlow.h"
#include "mlir/IR/MLIRContext.h"
#include "mlir/Parser/Parser.h"
#include "mlir/Transforms/Passes.h"
#include "mlir/Conversion/Passes.h"
#include "mlir/Pass/PassManager.h"
#include "mlir/Support/FileUtilities.h"
#include "mlir/Tools/mlir-opt/MlirOptMain.h"

#include "trsc/MLIR/TrscDialect.h"

#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/Arith/IR/Arith.h"

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
  registry.insert<mlir::memref::MemRefDialect>();
  registry.insert<mlir::scf::SCFDialect>();

  mlir::registerCanonicalizerPass();
  mlir::registerCSEPass();
  mlir::registerMem2RegPass();
  mlir::registerSCFToControlFlowPass();

  return mlir::asMainReturnCode(
      mlir::MlirOptMain(argc, argv, "TRSC optimizer driver\n", registry));
}
