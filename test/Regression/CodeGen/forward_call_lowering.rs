// RUN: %trsc -emit-mlir --matmul-opt-level=0 %s | %FileCheck %s

fn main() {
    print_doubled(21);
}

fn print_doubled(n: i32) {
    println!("doubled {}", n * 2);
}

// A call to a function defined later in the file lowers to a real func.call:
// MLIRGen declares every signature before emitting any body.
// CHECK-LABEL: func.func @main
// CHECK: call @print_doubled({{.*}}) : (i32) -> ()
// CHECK-LABEL: func.func @print_doubled
