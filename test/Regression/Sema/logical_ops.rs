// RUN: %trsc -dump-typedast %s | %FileCheck %s

fn main() {
    let a = true;
    let b = false;
    let c = a && b;
    let d = a || (1 == 1);
}

// CHECK-DAG: LetStmt {{0x[0-9a-f]+}} {{.*}} immut
// CHECK-DAG: VarExpr {{0x[0-9a-f]+}} {{.*}} 'c'
// CHECK-DAG: BinExpr {{0x[0-9a-f]+}} {{.*}} 'bool' 'OP_AMPAMP'
// CHECK-DAG: LetStmt {{0x[0-9a-f]+}} {{.*}} immut
// CHECK-DAG: VarExpr {{0x[0-9a-f]+}} {{.*}} 'd'
// CHECK-DAG: BinExpr {{0x[0-9a-f]+}} {{.*}} 'bool' 'OP_PIPEPIPE'
