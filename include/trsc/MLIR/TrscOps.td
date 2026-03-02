#ifndef TRSC_OPS_TD
#define TRSC_OPS_TD

include "trsc/MLIR/TrscDialect.td"
include "mlir/IR/OpBase.td"
include "mlir/Interfaces/SideEffectInterfaces.td"

def ConstantOp : Op<TrscDialect, "const", [ConstantLike, Pure]> {
  let summary = "Constant Operation";
  let arguments = (ins AnyAttr:$value);
  let results = (outs AnyType:$res);
  let assemblyFormat = "$value `->` type($res) attr-dict";
}

def AddOp : Op<TrscDialect, "add", [NativeOpTrait<"SameOperandsAndResultType">]> {
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

  let assemblyFormat = "$lhs `,` $rhs `:` type($lhs) `,` type($rhs) `->` type($result) attr-dict";
}

def SubOp : Op<TrscDialect, "sub", [NativeOpTrait<"SameOperandsAndResultType">]> {
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

  let assemblyFormat = "$lhs `,` $rhs `:` type($lhs) `,` type($rhs) `->` type($result) attr-dict";
}

def MulOp : Op<TrscDialect, "mul", [NativeOpTrait<"SameOperandsAndResultType">]> {
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

  let assemblyFormat = "$lhs `,` $rhs `:` type($lhs) `,` type($rhs) `->` type($result) attr-dict";
}

def DivOp : Op<TrscDialect, "div", [NativeOpTrait<"SameOperandsAndResultType">]> {
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

  let assemblyFormat = "$lhs `,` $rhs `:` type($lhs) `,` type($rhs) `->` type($result) attr-dict";
}

def CmpOp : Op<TrscDialect, "cmp", [NativeOpTrait<"SameTypeOperands">]> {
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

  let assemblyFormat = "$lhs `,` $rhs `:` type($lhs) `,` type($rhs) attr-dict";
}

#endif //TRSC_OPS_TD

