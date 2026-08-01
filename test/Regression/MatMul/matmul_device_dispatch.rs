// RUN: %trsc --device=auto --matmul-opt-level=6 -emit-mlir -optim=finopt %s | %FileCheck %s --check-prefix=AUTO
// RUN: %trsc --device=cpu --matmul-opt-level=6 -emit-mlir -optim=finopt %s | %FileCheck %s --check-prefix=CPU
// RUN: %trsc --device=cuda --cuda-arch=sm_80 --matmul-opt-level=2 -emit-mlir -optim=stdlowering %s | %FileCheck %s --check-prefix=CUDA

// AUTO: func.func private @trsc_cuda_is_available(i32) -> i32
// AUTO: call @trsc_cuda_is_available
// AUTO: scf.if
// AUTO: memref.copy
// AUTO: gpu.launch
// AUTO: call @trsc_cuda_had_error
// AUTO: memref.copy

// CPU-NOT: trsc_cuda
// CPU-NOT: gpu.launch
// CPU: scf.for

// CUDA: gpu.binary
// CUDA-SAME: chip = "sm_80"

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
