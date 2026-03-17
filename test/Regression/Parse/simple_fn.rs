// RUN: %trsc -dump-ast %s | %FileCheck %s

fn main() {
    let x = 5;
}

// CHECK: Program
// CHECK:  FuncDecl 'main'
// CHECK:  LetStmt
// CHECK:  VarExpr: 'x'
// CHECK:  IntExpr: 5
