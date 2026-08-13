// RUN: %trsc -optim=raw -emit-mlir --matmul-opt-level=0 %s | %FileCheck %s

fn side_effect() -> bool {
    println!("side effect");
    return true;
}

fn main() {
    let and_result = false && side_effect();
    let or_result = true || side_effect();
}

// RHS calls are placed in regions of scf.if, not emitted before the logical
// operation. Runtime execution therefore skips both calls for these operands.
// CHECK-LABEL: func.func @main
// CHECK:         scf.if
// CHECK:           func.call @side_effect
// CHECK:         scf.yield
// CHECK:         scf.if
// CHECK:           func.call @side_effect
// CHECK:         scf.yield
