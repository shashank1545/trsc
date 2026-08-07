// RUN: not %trsc %s 2>&1 | %FileCheck %s

fn side_effect() {
    return;
}

fn main() {
    // Unit has no storage; binding it used to reach MLIRGen and assert while
    // building a memref for the variable.
    let x = println!("hi");
    let y = side_effect();
}

// CHECK: Error: Cannot bind a value of type '()' to a variable
// CHECK: Error: Cannot bind a value of type '()' to a variable
// CHECK: Semantic analysis failed
