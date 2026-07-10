// RUN: %trsc --matmul-opt-level=4 -emit-mlir -optim=finopt %s | %FileCheck %s

// Level 4: 1D blocktiling. 64x8 thread block, rectangular SMEM tiles
// (64x8 for A, 8x64 for B), and a per-thread register accumulator strip
// (memref.alloca 8xf32) so each thread computes 8 outputs. The k dimension
// is processed in tiles of 8.

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
// CHECK-DAG:     %[[C8:.*]] = arith.constant 8 : index
// CHECK-DAG:     %[[C64:.*]] = arith.constant 64 : index
// CHECK:         gpu.launch blocks({{.*}}) threads({{.*}}) in (%{{.*}} = %[[C64]], %{{.*}} = %[[C8]], %{{.*}} = %{{.*}}) workgroup(%[[SA:.*]] : memref<64x8xf32, #gpu.address_space<workgroup>>, %[[SB:.*]] : memref<8x64xf32, #gpu.address_space<workgroup>>)
// CHECK-NOT:       memref.alloca
// CHECK:           scf.for %{{.*}} = %{{.*}} to %{{.*}} step %[[C8]] iter_args(
// CHECK:             memref.store %{{.*}}, %[[SA]]
// CHECK:             memref.store %{{.*}}, %[[SB]]
// CHECK:             gpu.barrier
// CHECK:             memref.load %[[SB]]
// CHECK:             memref.load %[[SA]]
// CHECK:             arith.mulf
// CHECK:             gpu.barrier
// CHECK:           gpu.terminator
