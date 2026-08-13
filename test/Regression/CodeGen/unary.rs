// RUN: %trsc -emit-mlir %s | %FileCheck %s

fn neg_i32(value: i32) -> i32 {
    return -value;
}

fn neg_f32(value: f32) -> f32 {
    return -value;
}

fn invert_bool(flag: bool) -> bool {
    return !flag;
}

fn main() {
}

// CHECK-LABEL: func.func @neg_i32(
// CHECK: arith.subi
// CHECK: return
// CHECK-LABEL: func.func @neg_f32(
// CHECK: arith.negf
// CHECK: return
// CHECK-LABEL: func.func @invert_bool(
// CHECK: arith.constant true
// CHECK: arith.xori
