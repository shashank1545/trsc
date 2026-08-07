// RUN: %trsc -dump-ast %s | %FileCheck %s

fn main() {
    let a: i32 = 1;
    let b: f64 = 2.5;
    println!("a={} b={}", a, b);
    println!("no arguments");
    println!();
}

// The dumped format string is the decoded one: the parser has already
// resolved escapes, and brace escapes are left for the format checker.
// CHECK: ExprStmt
// CHECK-NEXT: MacroCall println! format='a={} b={}'
// CHECK-NEXT: VarExpr: 'a'
// CHECK-NEXT: VarExpr: 'b'
// CHECK: ExprStmt
// CHECK-NEXT: MacroCall println! format='no arguments'
// CHECK: ExprStmt
// CHECK-NEXT: MacroCall println!
