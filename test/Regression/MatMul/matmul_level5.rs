// RUN: %trsc --matmul-opt-level=5 -emit-mlir -optim=finopt %s | %FileCheck %s

// Level 5: 2D blocktiling. 8x8 thread block where each thread computes an
// 8x8 register tile (memref.alloca 8x8xf32) fed from two per-thread register
// strips (8xf32 fragments of A and B), on top of the level-4 SMEM staging.

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
// CHECK:         %[[C8:.*]] = arith.constant 8 : index
// CHECK:         gpu.launch blocks({{.*}}) threads({{.*}}) in (%{{.*}} = %[[C8]], %{{.*}} = %[[C8]], %{{.*}} = %{{.*}}) workgroup(%[[SA:.*]] : memref<64x8xf32, #gpu.address_space<workgroup>>, %[[SB:.*]] : memref<8x64xf32, #gpu.address_space<workgroup>>)
// CHECK:           memref.alloca() : memref<8x8xf32>
// CHECK:           memref.alloca() : memref<8xf32>
// CHECK:           memref.alloca() : memref<8xf32>
// CHECK:           scf.for %{{.*}} = %{{.*}} to %{{.*}} step %[[C8]]
// CHECK:             memref.store %{{.*}}, %[[SA]]
// CHECK:             memref.store %{{.*}}, %[[SB]]
// CHECK:             gpu.barrier
// CHECK:             memref.load %[[SA]]
// CHECK:             memref.load %[[SB]]
// CHECK:           gpu.terminator
