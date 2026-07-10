// RUN: %trsc --matmul-opt-level=6 -emit-mlir -optim=finopt %s | %FileCheck %s

// Level 6: vectorization. Same 2D-blocktiled kernel as level 5, but the
// global-to-SMEM staging moves 4 floats at a time with vector.load, and the
// A tile is stored transposed (8x64) so per-thread fragments also load as
// vector<4xf32> from SMEM.

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
// CHECK:         gpu.launch blocks({{.*}}) threads({{.*}}) in (%{{.*}} = %[[C8]], %{{.*}} = %[[C8]], %{{.*}} = %{{.*}}) workgroup(%[[SA:.*]] : memref<8x64xf32, #gpu.address_space<workgroup>>, %[[SB:.*]] : memref<8x64xf32, #gpu.address_space<workgroup>>)
// CHECK:           vector.load %{{.*}} : memref<32x32xf32>, vector<4xf32>
// CHECK-COUNT-4:   memref.store %{{.*}}, %[[SA]]
// CHECK:           vector.load %{{.*}} : memref<32x32xf32>, vector<4xf32>
// CHECK:           vector.store %{{.*}}, %[[SB]]{{\[}}{{.*}} : memref<8x64xf32, #gpu.address_space<workgroup>>, vector<4xf32>
// CHECK:           gpu.barrier
// CHECK:           vector.load %[[SA]]
// CHECK:           vector.load %[[SB]]
// CHECK:           gpu.terminator
