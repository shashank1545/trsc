// RUN: %trsc -dump-ast %s | %FileCheck %s

fn main() {
    let mut sum = 0;
    for i in 0..10 {
        for j in 0..10 {
            for k in 0..10 {
                if i == j && j == k {
                    sum = sum + i + j + k;
                } else {
                    if i + j == k || i - j == k {
                        sum = sum - 1;
                    }
                }
            }
        }
    }
}

// CHECK-LABEL: FuncDecl 'main'
// CHECK: ForStmt: 
// CHECK:   Body: 
// CHECK:     ForStmt: 
// CHECK:       Body: 
// CHECK:         ForStmt: 
// CHECK:           Body: 
// CHECK:             IfStmt: 
// CHECK:               Condition: 
// CHECK:                 BinExpr: 'OP_AMPAMP'
// CHECK:                   BinExpr: 'OP_EQUALEQUAL'
// CHECK:                   BinExpr: 'OP_EQUALEQUAL'
// CHECK:               Then: 
// CHECK:                 ExprStmt
// CHECK:                   BinExpr: 'OP_EQUAL'
// CHECK:               Else:
// CHECK:                 IfStmt: 
// CHECK:                   Condition: 
// CHECK:                     BinExpr: 'OP_PIPEPIPE'
// CHECK:                       BinExpr: 'OP_EQUALEQUAL'
// CHECK:                       BinExpr: 'OP_EQUALEQUAL'
