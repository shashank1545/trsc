// RUN: %trsc -emit-mlir --matmul-opt-level=0 %s | %FileCheck %s

fn main() {
    let i: i64 = 1;
    let u: u32 = 2;
    let f: f32 = 3.5;
    let b = true;
    println!("i={} u={} f={} b={}", i, u, f, b);
    println!("i={}", i);
}

// Literal runs become one interned global plus one trsc_print_str call each,
// rather than a call per character. "i=" appears in both statements and is
// emitted once.
// CHECK-DAG: llvm.mlir.global internal constant @trsc_print_str.{{[0-9]+}}("i=")
// CHECK-DAG: llvm.mlir.global internal constant @trsc_print_str.{{[0-9]+}}(" u=")
// CHECK-DAG: func.func private @trsc_print_str(!llvm.ptr, i64)

// Scalars are widened to the runtime's ABI types: signed to i64, unsigned to
// i64 via zero-extension, bool to i32.
// CHECK-DAG: func.func private @trsc_print_i64(i64)
// CHECK-DAG: func.func private @trsc_print_u64(i64)
// CHECK-DAG: func.func private @trsc_print_f32(f32)
// CHECK-DAG: func.func private @trsc_print_bool(i32)
// CHECK-DAG: func.func private @trsc_print_newline()

// CHECK-LABEL: func.func @main
// CHECK-NOT: trsc_print_char
