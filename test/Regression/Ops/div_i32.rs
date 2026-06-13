// RUN: %trsc -dump-token %s | %FileCheck %s --check-prefix=TOKENS
// RUN: %trsc -dump-ast %s | %FileCheck %s --check-prefix=AST
// RUN: %trsc -dump-symboltable %s | %FileCheck %s --check-prefix=SYMBOLTABLE
// RUN: %trsc -dump-typedast %s | %FileCheck %s --check-prefix=TYPEDAST
// RUN: %trsc -emit-mlir %s | %FileCheck %s --check-prefix=MLIR

fn div_i32(a: i32, b: i32) -> i32 {
    return a / b;
}

// TOKENS: Token: KW_FN
// TOKENS: Token: IDENTIFIER Text: 'div_i32'
// TOKENS: Token: KW_RETURN
// TOKENS: Token: IDENTIFIER Text: 'a'
// TOKENS: Token: OP_SLASH
// TOKENS: Token: IDENTIFIER Text: 'b'

// AST: FuncDecl 'div_i32'
// AST: BinExpr: 'OP_SLASH'
// AST-NEXT: VarExpr: 'a'
// AST-NEXT: VarExpr: 'b'

// SYMBOLTABLE: ┌─ Global Scope (Depth: 0)
// SYMBOLTABLE: │  div_i32               : (i32,i32,) -> i32 [Function]

// TYPEDAST: FuncDecl {{.*}} 'div_i32'
// TYPEDAST: BinExpr {{.*}} 'i32' 'OP_SLASH'
// TYPEDAST: VarExpr {{.*}} 'i32' 'a'
// TYPEDAST: VarExpr {{.*}} 'i32' 'b'

// MLIR: func.func @{{.*}}div_i32(%arg0: i32, %arg1: i32) -> i32
// MLIR: {{.*}} = arith.divsi {{.*}}, {{.*}} : i32
// MLIR: return {{.*}} : i32
