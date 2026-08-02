// RUN: not %trsc %s 2>&1 | %FileCheck %s

fn main() {
    let x = y;
}

// CHECK: 4:13: Error: Undeclared variable
