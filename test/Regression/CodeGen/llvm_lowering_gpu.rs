// RUN: %trsc --matmul-opt-level=2 -emit-mlir -optim=stdlowering %s | %FileCheck %s --check-prefix=LLVMDIALECT
// RUN: %trsc --matmul-opt-level=2 -emit-llvm %s | %FileCheck %s --check-prefix=LLVMIR

// GPU path: the gpu.launch kernel is outlined, compiled to an embedded
// gpu.binary (PTX), and the host side becomes llvm-dialect code that
// registers memory and launches the kernel through the mgpu* runtime.

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

// --- MLIR llvm dialect + gpu binary (after -optim=stdlowering) ---

// LLVMDIALECT:       module attributes {gpu.container_module}
// LLVMDIALECT:       llvm.func @malloc(i64) -> !llvm.ptr
// LLVMDIALECT-LABEL: llvm.func @main() -> f32
// LLVMDIALECT:         llvm.call @malloc
// LLVMDIALECT-COUNT-3: llvm.call @mgpuMemHostRegisterMemRef
// LLVMDIALECT:         gpu.launch_func @main_kernel::@main_kernel
// LLVMDIALECT:         llvm.return %{{.*}} : f32
// LLVMDIALECT:       gpu.binary @main_kernel
// LLVMDIALECT:       llvm.func @mgpuMemHostRegisterMemRef(i64, !llvm.ptr, i64)

// --- Translated LLVM IR (-emit-llvm) ---

// LLVMIR: declare ptr @malloc(i64)
// LLVMIR: define float @main()
// LLVMIR-COUNT-3: call void @mgpuMemHostRegisterMemRef
// LLVMIR: call ptr @mgpuModuleGetFunction
// LLVMIR: call ptr @mgpuStreamCreate
// LLVMIR: call void @mgpuLaunchKernel
// LLVMIR: call void @mgpuStreamSynchronize
// LLVMIR: call void @mgpuStreamDestroy
// LLVMIR: ret float
// LLVMIR: define internal void @main_kernel_load() section ".text.startup"
// LLVMIR: call ptr @mgpuModuleLoadJIT
// LLVMIR: define internal void @main_kernel_unload() section ".text.startup"
// LLVMIR: call void @mgpuModuleUnload
