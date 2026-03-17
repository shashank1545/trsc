// RUN: %trsc -dump-ast %s | %FileCheck %s

fn add(a: i32, b: i32) -> i32 {
    return a + b;
}

fn main() {
    let x = 5;
    let mut y: i32 = 10;
    let z = add(x, y);
}

// CHECK-LABEL: Program
// CHECK:  FuncDecl 'add'
// CHECK:    ReturnStmt
// CHECK:      BinExpr: 'OP_PLUS'
// CHECK:        VarExpr: 'a'
// CHECK:        VarExpr: 'b'

// CHECK:  FuncDecl 'main'
// CHECK:    LetStmt
// CHECK:      VarExpr: 'x'
// CHECK:      IntExpr: 5
// CHECK:    LetStmt
// CHECK:      VarExpr: 'y'
// CHECK:      TypeName: 'i32'
// CHECK:      IntExpr: 10
// CHECK:    LetStmt
// CHECK:      VarExpr: 'z'
// CHECK:      FunCalladd
// CHECK:        VarExpr: 'x'
// CHECK:        VarExpr: 'y'
