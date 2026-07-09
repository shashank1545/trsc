#ifndef TRSC_OPS_TD
#define TRSC_OPS_TD

include "trsc/MLIR/TrscDialect.td"
include "mlir/IR/OpBase.td"
include "mlir/Interfaces/InferTypeOpInterface.td"
include "mlir/Interfaces/SideEffectInterfaces.td"

def ConstantOp : Op<TrscDialect, "const", [ConstantLike, Pure]> {
  let summary = "Constant Operation";
  let arguments = (ins AnyAttr:$value);
  let results = (outs AnyType:$res);
  let assemblyFormat = "$value `->` type($res) attr-dict";
}

def AddOp : Op<TrscDialect, "add", [
  Pure,
  Commutative,
  AllTypesMatch<["lhs", "rhs", "result"]>, 
  DeclareOpInterfaceMethods<InferTypeOpInterface>
]> {
  let summary = "Simple numeric addition";
  let description = [{
    Adds two numeric values of the same type.
  }];

  let arguments = (ins
      AnyType:$lhs,
      AnyType:$rhs
      );

  let results = (outs
      AnyType:$result
      );

  let assemblyFormat = "$lhs `,` $rhs `:` type($lhs) attr-dict";

  // let hasFolder = 1;
}

def SubOp : Op<TrscDialect, "sub", [
  Pure,
  AllTypesMatch<["lhs", "rhs", "result"]>,
  DeclareOpInterfaceMethods<InferTypeOpInterface>
]> {
  let summary = "Simple numeric subtraction";
  let description = [{
    Subtracts a number from another of the same type.
  }];

  let arguments = (ins 
      AnyType:$lhs,
      AnyType:$rhs
      );

  let results = (outs 
      AnyType:$result
      );

  let assemblyFormat = "$lhs `,` $rhs `:` type($lhs) attr-dict";
}

def MulOp : Op<TrscDialect, "mul", [
  Pure,
  Commutative,
  AllTypesMatch<["lhs", "rhs", "result"]>, 
  DeclareOpInterfaceMethods<InferTypeOpInterface>
]> {
  let summary = "Simple numeric multiplication";
  let description = [{
    Multiplies two numeric values of the same type.
  }];

  let arguments = (ins 
      AnyType:$lhs,
      AnyType:$rhs
      );

  let results = (outs 
      AnyType:$result
      );

  let assemblyFormat = "$lhs `,` $rhs `:` type($lhs) attr-dict";
}

def DivOp : Op<TrscDialect, "div", [
  Pure,
  AllTypesMatch<["lhs", "rhs", "result"]>,
  DeclareOpInterfaceMethods<InferTypeOpInterface>
]> {
  let summary = "Simple numeric subtraction";
  let description = [{
    Divides a number from another of the same type.
  }];

  let arguments = (ins 
      AnyType:$lhs,
      AnyType:$rhs
      );

  let results = (outs 
      AnyType:$result
      );

  let assemblyFormat = "$lhs `,` $rhs `:` type($lhs) attr-dict";
}

def CmpOp : Op<TrscDialect, "cmp", [AllTypesMatch<["lhs", "rhs"]>]> {
  let summary = "Basic Comparison between numeric";
  let description = [{
    Compares two numbers of the same type.
  }];

  let arguments = (ins 
      AnyType:$lhs,
      AnyType:$rhs
      );

  let results = (outs
      I1:$result
      );

  let assemblyFormat = "$lhs `,` $rhs `:` type($lhs) attr-dict";
}

def ReturnOp : Op<TrscDialect, "ret" , [Terminator, Pure]> {
  let summary = "Return for a function.";
  let description = [{
    The "ret" operation terminates a block and yields value back to the caller.
  }];

  let arguments = (ins Variadic<AnyType>:$operands);

  let assemblyFormat = "($operands^ `:` type($operands))? attr-dict";
}

//===----------------------------------------------------------------------===//
// MatMul / GEMM Operations
//===----------------------------------------------------------------------===//

def GemmOp : Op<TrscDialect, "gemm", []> {
  let summary = "High-level GEMM operation";
  let description = [{
    The `trscd.gemm` operation represents a dense matrix multiplication:
    C = alpha * A * B + beta * C.
  }];

  let arguments = (ins
    AnyMemRef:$A,
    AnyMemRef:$B,
    AnyMemRef:$C,
    DefaultValuedAttr<F32Attr, "1.0">:$alpha,
    DefaultValuedAttr<F32Attr, "0.0">:$beta,
    OptionalAttr<I64Attr>:$M,
    OptionalAttr<I64Attr>:$N,
    OptionalAttr<I64Attr>:$K,
    OptionalAttr<DictionaryAttr>:$tiling_params
  );

  let assemblyFormat = "`(` $A `,` $B `,` $C `)` attr-dict `:` "
                       "type($A) `,` type($B) `,` type($C)";
}

#endif //TRSC_OPS_TD

