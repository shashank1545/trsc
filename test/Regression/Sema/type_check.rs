// RUN: %trsc -dump-typedast %s | %FileCheck %s

fn main() {
    let x = 5;
    let y = 10;
    let z = x + y;
}

// CHECK: Program {{0x[0-9a-f]+}} <{{[0-9:]+}}, {{[0-9:]+}}>
// CHECK:  └── FuncDecl {{0x[0-9a-f]+}} <{{[0-9:]+}}, {{[0-9:]+}}> 'main'
// CHECK:      └── BlockStmt {{0x[0-9a-f]+}} <{{[0-9:]+}}, {{[0-9:]+}}>
// CHECK:          ├── LetStmt {{0x[0-9a-f]+}} <{{[0-9:]+}}, {{[0-9:]+}}> immut
// CHECK:          │   ├── VarExpr {{0x[0-9a-f]+}} <{{[0-9:]+}}> 'x'
// CHECK:          │   └── IntExpr {{0x[0-9a-f]+}} <{{[0-9:]+}}> 'i32' 5
// CHECK:          ├── LetStmt {{0x[0-9a-f]+}} <{{[0-9:]+}}, {{[0-9:]+}}> immut
// CHECK:          │   ├── VarExpr {{0x[0-9a-f]+}} <{{[0-9:]+}}> 'y'
// CHECK:          │   └── IntExpr {{0x[0-9a-f]+}} <{{[0-9:]+}}> 'i32' 10
// CHECK:          └── LetStmt {{0x[0-9a-f]+}} <{{[0-9:]+}}, {{[0-9:]+}}> immut
// CHECK:              ├── VarExpr {{0x[0-9a-f]+}} <{{[0-9:]+}}> 'z'
// CHECK:              └── BinExpr {{0x[0-9a-f]+}} <{{[0-9:]+}}, {{[0-9:]+}}> 'i32' 'OP_PLUS'
// CHECK:                  ├── VarExpr {{0x[0-9a-f]+}} <{{[0-9:]+}}> 'i32' 'x'
// CHECK:                  └── VarExpr {{0x[0-9a-f]+}} <{{[0-9:]+}}> 'i32' 'y'
