// RUN: %trsc -emit-mlir -optim=stdlowering %s | %FileCheck %s --check-prefix=LLVMDIALECT
// RUN: %trsc -emit-llvm %s | %FileCheck %s --check-prefix=LLVMIR

// CPU-only path: the full pipeline lowers scf control flow to the llvm
// dialect (branch-based CFG), then translates to LLVM IR proper. The loop is
// unrolled by 4 by the late loop optimizations before lowering.

fn main() -> f32 {
    let mut acc: f32 = 0.0;
    for i in 0..8 {
        acc = acc + 2.0;
    }
    return acc;
}

// --- MLIR llvm dialect (after -optim=stdlowering) ---

// LLVMDIALECT-LABEL: llvm.func @main() -> f32
// LLVMDIALECT-NOT:     scf.for
// LLVMDIALECT:         llvm.br ^bb1
// LLVMDIALECT:       ^bb1(%{{.*}}: i64, %{{.*}}: f32):
// LLVMDIALECT:         llvm.icmp "slt"
// LLVMDIALECT:         llvm.cond_br
// LLVMDIALECT-COUNT-4: llvm.fadd
// LLVMDIALECT-NOT:     scf.
// LLVMDIALECT:         llvm.return %{{.*}} : f32

// --- Translated LLVM IR (-emit-llvm) ---

// LLVMIR: define float @main()
// LLVMIR: phi i64
// LLVMIR: phi float
// LLVMIR: icmp slt i64
// LLVMIR: br i1
// LLVMIR-COUNT-4: fadd float
// LLVMIR: ret float
