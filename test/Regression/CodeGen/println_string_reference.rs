// RUN: %trsc -emit-mlir --matmul-opt-level=0 %s | %FileCheck %s

fn show(value: &i32) {
    println!("{}", value);
}

fn show_mut(value: &mut i32) {
    println!("{}", value);
}

fn main() {
    let mut value: i32 = 7;
    println!("{}", "world");
    show(&value);
    show_mut(&mut value);
}

// String values reuse the literal pointer/length runtime path.
// CHECK-DAG: llvm.mlir.global internal constant @trsc_print_str.{{[0-9]+}}("world")
// CHECK-DAG: func.func private @trsc_print_str(!llvm.ptr, i64)

// Both reference kinds are loaded before the scalar formatter is called.
// CHECK-DAG: func.func private @trsc_print_i64(i64)
// CHECK: func.func @show({{.*}}memref<i32>)
// CHECK: func.func @show_mut({{.*}}memref<i32>)
