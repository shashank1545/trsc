// RUN: not %trsc -dump-token %s 2>&1 | %FileCheck %s

fn main() {
    println!("bad \q escape");
    println!("no closing quote);
}

// CHECK: Error: Unknown escape sequence '\q'
// CHECK: Error: Unterminated string literal
// CHECK: Lexing failed
