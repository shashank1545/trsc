#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/Linalg/IR/Linalg.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/AsmState.h"
#include "mlir/IR/MLIRContext.h"
#include "mlir/Tools/mlir-opt/MlirOptMain.h"
#include "mlir/Transforms/Passes.h"

#include "trsc/MLIR/MatMulOpts/MatMulOptPasses.h"
#include "trsc/MLIR/TrscDialect.h"
#include "trsc/MLIR/TrscPasses.h"

int main(int argc, char **argv) {
  // Register MLIR command line options
  mlir::registerMLIRContextCLOptions();
  mlir::registerPassManagerCLOptions();
  mlir::registerAsmPrinterCLOptions();

  mlir::DialectRegistry registry;
  registry.insert<mlir::func::FuncDialect>();
  registry.insert<mlir::arith::ArithDialect>();
  registry.insert<mlir::memref::MemRefDialect>();
  registry.insert<mlir::scf::SCFDialect>();
  registry.insert<mlir::linalg::LinalgDialect>();
  registry.insert<mlir::trscd::TrscDialect>();

  mlir::registerCanonicalizerPass();
  mlir::registerCSEPass();
  mlir::registerMem2RegPass();

  mlir::trscd::registerTrscPasses();
  mlir::trscd::registerMatMulOptPasses();

  return mlir::asMainReturnCode(
      mlir::MlirOptMain(argc, argv, "TRSC optimizer driver\n", registry));
}
