// RUN: %trsc -dump-token %s | %FileCheck %s

// This is a comment at the start
fn main() { // comment after code
    let x = 1; // let x = 2; (this should be ignored)
    /* This style might not be supported but let's check line comments mostly */
    let y = 3; 
}
// Comment at the end

// CHECK: Token: KW_FN Text: 'fn'
// CHECK: Token: IDENTIFIER Text: 'main'
// CHECK: Token: DE_LPAREN Text: '('
// CHECK: Token: DE_RPAREN Text: ')'
// CHECK: Token: DE_LBRACE Text: '{'
// CHECK: Token: KW_LET Text: 'let'
// CHECK: Token: IDENTIFIER Text: 'x'
// CHECK: Token: OP_EQUAL Text: '='
// CHECK: Token: LT_INTEGER Text: '1'
// CHECK: Token: DE_SEMICOLON Text: ';'
// CHECK: Token: KW_LET Text: 'let'
// CHECK: Token: IDENTIFIER Text: 'y'
// CHECK: Token: OP_EQUAL Text: '='
// CHECK: Token: LT_INTEGER Text: '3'
// CHECK: Token: DE_SEMICOLON Text: ';'
// CHECK: Token: DE_RBRACE Text: '}'
// CHECK: Token: ENDOFFILE Text: ''
