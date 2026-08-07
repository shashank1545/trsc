// RUN: %trsc -dump-token %s | %FileCheck %s

// An escaped quote stays inside the literal instead of terminating it, so the
// whole string is one LT_STRING token and the following ')' still lexes.
fn main() {
    println!("say \"hi\" and \\ then \t tab");
}

// CHECK: Token: IDENTIFIER Text: 'println'
// CHECK-NEXT: Token: OP_BANG Text: '!'
// CHECK-NEXT: Token: DE_LPAREN Text: '('
// CHECK-NEXT: Token: LT_STRING Text: '"say \"hi\" and \\ then \t tab"'
// CHECK-NEXT: Token: DE_RPAREN Text: ')'
// CHECK-NEXT: Token: DE_SEMICOLON Text: ';'
