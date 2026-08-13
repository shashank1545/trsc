// RUN: %trsc -optim=raw -emit-mlir %s | %FileCheck %s

fn recursion_variable_argument(value: i32) -> i32 {
    if value <= 0 {
        return 0;
    }
    return recursion_variable_argument(value - 1);
}

fn main() {}

// CHECK-LABEL: func.func @recursion_variable_argument
// CHECK:         scf.if
// CHECK:           scf.yield
// CHECK:         func.call @recursion_variable_argument
// CHECK:         return
