// RUN: not %trsc %s 2>&1 | %FileCheck %s

fn test_return() -> i32 {
    return true;
}

fn main() {
    let x: i32 = false;
}

// CHECK: Return type mismatch: expected 'i32', found 'bool'
// CHECK: Type mismatch: cannot initialize variable of type 'i32' with value of type 'bool'
