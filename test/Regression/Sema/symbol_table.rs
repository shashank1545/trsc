// RUN: %trsc -dump-symboltable %s | %FileCheck %s

fn main() {
}

// CHECK: SYMBOL TABLE TREE
// CHECK: ┌─ Global Scope (Depth: 0)
// CHECK: │  main                 : () -> () [Function]
