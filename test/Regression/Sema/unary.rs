// RUN: %trsc -dump-typedast %s | %FileCheck %s

fn main() {
    let value: i32 = -3;
    let float_value: f32 = -0.0;
    let flag: bool = !false;
}

// CHECK: UnaryExpr {{0x[0-9a-f]+}} <{{[0-9:]+}}, {{[0-9:]+}}> 'i32' 'OP_MINUS'
// CHECK: IntExpr {{0x[0-9a-f]+}} <{{[0-9:]+}}, {{[0-9:]+}}> 'i32' 3
// CHECK: UnaryExpr {{0x[0-9a-f]+}} <{{[0-9:]+}}, {{[0-9:]+}}> 'f32' 'OP_MINUS'
// CHECK: FloatExpr {{0x[0-9a-f]+}} <{{[0-9:]+}}, {{[0-9:]+}}> 'f32' 0
// CHECK: UnaryExpr {{0x[0-9a-f]+}} <{{[0-9:]+}}, {{[0-9:]+}}> 'bool' 'OP_BANG'
// CHECK: BoolExpr {{0x[0-9a-f]+}} <{{[0-9:]+}}, {{[0-9:]+}}> 'bool' false
