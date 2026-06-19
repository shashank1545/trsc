#ifndef TRSC_DIALECT
#define TRSC_DIALECT

include "mlir/IR/OpBase.td"

def TrscDialect : Dialect {
    let name = "trscd";
    let summary = "A high level dialect for my rust compiler.";
    let cppNamespace = "::mlir::trscd";
  }

#endif // TRSC_DIALECT


