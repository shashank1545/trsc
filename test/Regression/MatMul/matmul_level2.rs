// RUN: %trsc --matmul-opt-level=2 -emit-mlir -optim=finopt %s | %FileCheck %s

// Level 2: naive GPU kernel with GMEM coalescing. One thread per output
// element (32x32 thread block), bounds guard, k-loop reading straight from
// global memory (no shared-memory workgroup buffers yet).

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
// CHECK:         %[[C32:.*]] = arith.constant 32 : index
// CHECK-COUNT-3: gpu.alloc async
// CHECK:         gpu.launch blocks({{.*}}) in ({{.*}}) threads({{.*}}) in (%{{.*}} = %[[C32]], %{{.*}} = %[[C32]], %{{.*}} = %{{.*}})
// CHECK:           arith.cmpi ult
// CHECK:           arith.cmpi ult
// CHECK:           arith.andi
// CHECK:           scf.if
// CHECK:             scf.for
// CHECK:               memref.load %{{.*}} : memref<32x32xf32>
// CHECK:               memref.load %{{.*}} : memref<32x32xf32>
// CHECK:               arith.mulf
// CHECK:               arith.addf
// CHECK:               memref.store %{{.*}} : memref<32x32xf32>
// CHECK:           gpu.terminator
// CHECK:         } {trscd.gemm_generated}
