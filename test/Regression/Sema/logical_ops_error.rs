// RUN: not %trsc %s 2>&1 | %FileCheck %s

fn main() {
    let x = 1 && true;
}

// CHECK: Operator '&&' requires boolean operands ('i32' and 'bool')
