// RUN: %trsc --matmul-opt-level=1 -emit-mlir -optim=finopt %s | %FileCheck %s

// Level 1: GEMM recognized and lowered back to a canonical sequential CPU
// nest (i, j, then k with an iter_args accumulator). The nest carries the
// trscd.gemm_generated marker so the generic tiling/unrolling skips it.

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
// CHECK:         memref.load
// CHECK:         scf.for {{.*}} iter_args(
// CHECK:           arith.mulf
// CHECK:           arith.addf
// CHECK:           scf.yield
// CHECK:         }
// CHECK:         memref.store
// CHECK:         } {trscd.gemm_generated}
// CHECK:         return
