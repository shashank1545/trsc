// RUN: %trsc --matmul-opt-level=3 -emit-mlir -optim=finopt %s | %FileCheck %s

// Level 3: shared-memory caching. The kernel gets two workgroup (SMEM)
// buffers; threads cooperatively stage A and B tiles, synchronize with
// gpu.barrier, and the k-loop reads from SMEM instead of global memory.

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
// CHECK-COUNT-3: gpu.host_register
// CHECK:         gpu.launch blocks({{.*}}) workgroup(%[[SA:.*]] : memref<32x32xf32, #gpu.address_space<workgroup>>, %[[SB:.*]] : memref<32x32xf32, #gpu.address_space<workgroup>>)
// CHECK:           memref.store %{{.*}}, %[[SA]]
// CHECK:           memref.store %{{.*}}, %[[SB]]
// CHECK:           gpu.barrier
// CHECK:           scf.for {{.*}} iter_args(
// CHECK:             memref.load %[[SA]]
// CHECK:             memref.load %[[SB]]
// CHECK:             arith.mulf
// CHECK:             arith.addf
// CHECK:           gpu.barrier
// CHECK:           gpu.terminator
