// RUN: %trsc --matmul-opt-level=9 -emit-mlir -optim=finopt %s | %FileCheck %s

// Level 9: autotuning. Recognition raises the nest to trscd.gemm and the
// AutoTuning pass attaches the selected tiling parameters as attributes.
// NOTE: GemmLowering currently has no branch for level 9, so the trscd.gemm
// op survives finopt un-lowered — this test locks in the recognition +
// autotune contract only.

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
// CHECK:         trscd.gemm(%{{.*}}, %{{.*}}, %{{.*}})
// CHECK-SAME:      tiling_params
// CHECK-SAME:      tile_K
// CHECK-SAME:      tile_M
// CHECK-SAME:      tile_N
// CHECK-SAME:      vectorize_width
// CHECK:         return
