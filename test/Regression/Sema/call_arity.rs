// RUN: not %trsc %s 2>&1 | %FileCheck %s

fn main() {
    // Too many arguments used to index past the end of the declared parameter
    // list rather than being reported.
    add(1, 2, 3);
    add(1);
}

fn add(a: i32, b: i32) -> i32 {
    return a + b;
}

// CHECK: Error: Function add expects 2 argument(s) but got 3
// CHECK: Error: Function add expects 2 argument(s) but got 1
