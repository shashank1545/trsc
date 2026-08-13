// RUN: %trsc -optim=raw -emit-mlir %s | %FileCheck %s

fn conditional_return(value: i32) -> i32 {
    if value <= 0 {
        return 0;
    }
    return value;
}

fn main() {}

// CHECK-LABEL: func.func @conditional_return
// CHECK:         scf.if
// CHECK:           scf.yield
// CHECK:         return
