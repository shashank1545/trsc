// RUN: %trsc -dump-token %s | %FileCheck %s --check-prefix=TOKENS
// RUN: %trsc -dump-ast %s | %FileCheck %s --check-prefix=AST
// RUN: %trsc -dump-symboltable %s | %FileCheck %s --check-prefix=SYMBOLTABLE
// RUN: %trsc -dump-typedast %s | %FileCheck %s --check-prefix=TYPEDAST
// RUN: %trsc -emit-mlir %s | %FileCheck %s --check-prefix=MLIR

fn lt_u16(a: u16, b: u16) -> bool {
    return a < b;
}

// TOKENS: Token: KW_FN
// TOKENS: Token: IDENTIFIER Text: 'lt_u16'
// TOKENS: Token: KW_RETURN
// TOKENS: Token: IDENTIFIER Text: 'a'
// TOKENS: Token: OP_LESS
// TOKENS: Token: IDENTIFIER Text: 'b'

// AST: FuncDecl 'lt_u16'
// AST: BinExpr: 'OP_LESS'
// AST-NEXT: VarExpr: 'a'
// AST-NEXT: VarExpr: 'b'

// SYMBOLTABLE: ┌─ Global Scope (Depth: 0)
// SYMBOLTABLE: │  lt_u16               : (u16,u16,) -> bool [Function]

// TYPEDAST: FuncDecl {{.*}} 'lt_u16'
// TYPEDAST: BinExpr {{.*}} 'bool' 'OP_LESS'
// TYPEDAST: VarExpr {{.*}} 'u16' 'a'
// TYPEDAST: VarExpr {{.*}} 'u16' 'b'

// MLIR: func.func @{{.*}}lt_u16(%arg0: i16, %arg1: i16) -> i1
// MLIR: {{.*}} = arith.cmpi ult, {{.*}}, {{.*}} : i16
// MLIR: return {{.*}} : i1
