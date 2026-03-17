// RUN: %trsc -dump-ast %s | %FileCheck %s

fn main() {
    if true {
        let x = 1;
    } else {
        let x = 2;
    }

    while x < 10 {
        let y = x + 1;
    }

    for i in 0..10 {
        let z = i;
    }
}

// CHECK-LABEL: FuncDecl 'main'
// CHECK:  IfStmt: 
// CHECK:    Condition: 
// CHECK:      BoolExpr: true
// CHECK:    Then: 
// CHECK:      LetStmt
// CHECK:        VarExpr: 'x'
// CHECK:        IntExpr: 1
// CHECK:    Else:
// CHECK:      LetStmt
// CHECK:        VarExpr: 'x'
// CHECK:        IntExpr: 2

// CHECK:  WhileStmt
// CHECK:    Condition:
// CHECK:      BinExpr: 'OP_LESS'
// CHECK:        VarExpr: 'x'
// CHECK:        IntExpr: 10
// CHECK:    Body:
// CHECK:      LetStmt
// CHECK:        VarExpr: 'y'
// CHECK:        BinExpr: 'OP_PLUS'

// CHECK:  ForStmt: 
// CHECK:    Init: 
// CHECK:      VarExpr: 'i'
// CHECK:    Range: 
// CHECK:      RangeExpr 0
// CHECK:        IntExpr: 0
// CHECK:        IntExpr: 10
// CHECK:    Body: 
// CHECK:      LetStmt
// CHECK:        VarExpr: 'z'
// CHECK:        VarExpr: 'i'
