#ifndef TRSC_PASSES_TD
#define TRSC_PASSES_TD

include "mlir/Pass/PassBase.td"

//def LowerTrscToStandard : Pass<"lower-trsc-to-standard", "mlir::ModuleOp"> {
//  let summary = "Lower the TRSC dialect to standard dialects.";
//  let description = [{
//    This pass lowers the TRSC dialect to a combination of standard dialects,
//    like Func, Arith, and StructuredControlFlow.
//  }];
//
//  let constructor = "trsc::createTrscToStandard()";
//  
//  let dependentDialects = [
//    "mlir::arith::ArithDialect",
//    "mlir::func::FuncDialect",
//    "mlir::scf::SCFDialect"
//  ];
//}

def TrscLICM : Pass<"trsc-licm", "mlir::func::FuncOp"> {
    let summary = "Move loop invariant code out of scf.for loops.";
    let description = [{
      This pass moves loop invariant computation into their respective pre-
      headers.
      }];

    let constructor = "trsc::createTrscLICM()";

    let dependentDialects = ["mlir::scf::SCFDialect"];
  }

#endif // TRSC_PASSES_TD
