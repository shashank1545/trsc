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

    let constructor = "mlir::trscd::createTrscLICM()";

    let dependentDialects = ["mlir::scf::SCFDialect"];
  }

def TrscLoopFusion : Pass<"trsc-loopfusion", "mlir::func::FuncOp"> {
    let summary = "Combine adjacent loops that share the same iteration space.";
    let description = [{
      This pass fuses adjacent loops together into a single loop body to reduce
      loop overhead and improve data locality.
    }];

    let constructor = "mlir::trscd::createTrscLoopFusion()";
    let dependentDialects = ["mlir::scf::SCFDialect"];
  }

def TrscLoopTile : Pass<"trsc-loop-tile", "mlir::func::FuncOp"> {
    let summary = "Tile perfectly nested scf.for loops on host code.";
    let description = [{
      This pass tiles perfectly nested scf.for loop bands to improve cache
      locality. Loop nests produced by GEMM lowering (marked with the
      trscd.gemm_generated attribute) are already tiled for the target and
      are skipped.
    }];

    let constructor = "mlir::trscd::createTrscLoopTile()";
    let dependentDialects = ["mlir::scf::SCFDialect",
                             "mlir::arith::ArithDialect"];

    let options = [
      Option<"tileSize", "tile-size", "unsigned", "32",
             "Tile size applied to every dimension of a tiled band">
    ];
  }

def TrscLoopUnroll : Pass<"trsc-loop-unroll", "mlir::func::FuncOp"> {
    let summary = "Unroll innermost scf.for loops on host code.";
    let description = [{
      This pass unrolls innermost scf.for loops by a fixed factor to expose
      instruction-level parallelism. Loop nests produced by GEMM lowering
      (marked with the trscd.gemm_generated attribute) are skipped.
    }];

    let constructor = "mlir::trscd::createTrscLoopUnroll()";
    let dependentDialects = ["mlir::scf::SCFDialect",
                             "mlir::arith::ArithDialect"];

    let options = [
      Option<"unrollFactor", "unroll-factor", "unsigned", "4",
             "Unroll factor applied to innermost loops">
    ];
  }

#endif // TRSC_PASSES_TD
