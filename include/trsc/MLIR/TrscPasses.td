#ifndef TRSC_PASSES_TD
#define TRSC_PASSES_TD

include "mlir/Pass/PassBase.td"

def LowerTrscToStandard : Pass<"lower-trsc-to-standard", "mlir::ModuleOp"> {
  let summary = "Lower the TRSC dialect to standard dialects.";
  let description = [{
    This pass lowers the TRSC dialect to a combination of standard dialects,
    like Func, Arith, and ControlFlow.
  }];

  let constructor = "trsc::createTrscToStandardPass()";
  
  let dependentDialects = [
    "mlir::arith::ArithDialect",
    "mlir::func::FuncDialect",
    "mlir::cf::ControlFlowDialect"
  ];
}

#endif // TRSC_PASSES_TD
