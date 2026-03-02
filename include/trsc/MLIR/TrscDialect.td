#ifndef TRSC_DIALECT
#define TRSC_DIALECT

include "mlir/IR/OpBase.td"

def TrscDialect : Dialect {
    let name = "trsc";
    let summary = "A high level dialect for my rust compiler.";
    let cppNamespace = "::trsc";
    let hasConstantMaterializer = 1;
  }

#endif // TRSC_DIALECT


