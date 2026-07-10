// RUN: %trsc --matmul-opt-level=0 -emit-mlir -optim=finopt %s | %FileCheck %s

// Level 0: matmul optimization disabled. The loop nest is NOT recognized as a
// GEMM — it stays a plain scf.for nest and goes through the generic late loop
// optimizations instead (innermost loop unrolled by 4).

fn main() -> f32 {
    let a: [[f32; 32]; 32] = [[1.0; 32]; 32];
    let b: [[f32; 32]; 32] = [[2.0; 32]; 32];
    let mut c: [[f32; 32]; 32] = [[0.0; 32]; 32];

    for i in 0..32 {
        for j in 0..32 {
            for k in 0..32 {
                c[i][j] = c[i][j] + a[i][k] * b[k][j];
            }
        }
    }
    return c[0][0];
}

// CHECK-LABEL: func.func @main
// CHECK-NOT:     gpu.launch
// CHECK:         scf.for
// CHECK:         scf.for
// CHECK:         scf.for
// CHECK-COUNT-4:   arith.mulf
// CHECK-NOT:     trscd.gemm
// CHECK:         return
