// RUN: not %trsc %s 2>&1 | %FileCheck %s

fn main() {
    let a: i32 = 1;
    let arr: [i32; 2] = [1, 2];

    // Too few arguments for the placeholders.
    println!("{} {}", a);

    // More arguments than placeholders.
    println!("{}", a, a);

    // Unterminated placeholder.
    println!("{", a);

    // Unsupported format spec.
    println!("{:>8}", a);

    // Arguments without a format string.
    println!(a);

    // Aggregates have no Display form.
    println!("{}", arr);

    // Only println! exists so far.
    print!("hello");
}

// Name resolution runs over the whole tree before type checking, so the
// unsupported-macro diagnostic is reported ahead of the format diagnostics.
// CHECK: Error: Unsupported macro 'print!'
// CHECK: Error: println! expects 2 argument(s) for its placeholders but got 1
// CHECK: Error: println! expects 1 argument(s) for its placeholders but got 2
// CHECK: Error: Invalid println! format string
// CHECK: Error: Invalid println! format string
// CHECK: Error: println! requires a string literal format
// CHECK: Error: println! cannot format a value of type '[i32; 2]'
// CHECK: Semantic analysis failed with 7 errors
