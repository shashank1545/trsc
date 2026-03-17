// RUN: %trsc -dump-token %s | %FileCheck %s

fn main() {
    let x = 5;
    let y = 10;
    let z = x + y;
}

// CHECK: Token: KW_FN Text: 'fn'
// CHECK: Token: IDENTIFIER Text: 'main'
// CHECK: Token: DE_LPAREN Text: '('
// CHECK: Token: DE_RPAREN Text: ')'
// CHECK: Token: DE_LBRACE Text: '{'
// CHECK: Token: KW_LET Text: 'let'
// CHECK: Token: IDENTIFIER Text: 'x'
// CHECK: Token: OP_EQUAL Text: '='
// CHECK: Token: LT_INTEGER Text: '5'
// CHECK: Token: DE_SEMICOLON Text: ';'
// CHECK: Token: KW_LET Text: 'let'
// CHECK: Token: IDENTIFIER Text: 'y'
// CHECK: Token: OP_EQUAL Text: '='
// CHECK: Token: LT_INTEGER Text: '10'
// CHECK: Token: DE_SEMICOLON Text: ';'
// CHECK: Token: KW_LET Text: 'let'
// CHECK: Token: IDENTIFIER Text: 'z'
// CHECK: Token: OP_EQUAL Text: '='
// CHECK: Token: IDENTIFIER Text: 'x'
// CHECK: Token: OP_PLUS Text: '+'
// CHECK: Token: IDENTIFIER Text: 'y'
// CHECK: Token: DE_SEMICOLON Text: ';'
// CHECK: Token: DE_RBRACE Text: '}'
