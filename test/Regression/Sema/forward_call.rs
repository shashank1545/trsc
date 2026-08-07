// RUN: %trsc -dump-typedast %s | %FileCheck %s

fn main() {
    // Both callees are declared below this point. Signatures are resolved in a
    // pre-pass, so the calls are typed rather than silently left untyped.
    let doubled: i64 = twice(21);
    println!("{}", doubled);
    shout(doubled);
}

fn twice(n: i64) -> i64 {
    return n * 2;
}

fn shout(n: i64) {
    println!("value {}", n);
}

// The call takes the callee's return type, and the literal argument adopts the
// declared parameter type instead of falling back to i32.
// CHECK: FunCall {{.*}} 'twice' 'i64'
// CHECK: IntExpr {{.*}} 'i64' 21
// CHECK: FunCall {{.*}} 'shout' '()'
