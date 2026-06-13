// RUN: %trsc -dump-token %s | %FileCheck %s --check-prefix=TOKENS
// RUN: %trsc -dump-ast %s | %FileCheck %s --check-prefix=AST
// RUN: %trsc -dump-symboltable %s | %FileCheck %s --check-prefix=SYMBOLTABLE
// RUN: %trsc -dump-typedast %s | %FileCheck %s --check-prefix=TYPEDAST
// RUN: %trsc -emit-mlir %s | %FileCheck %s --check-prefix=MLIR

fn sub_i64(a: i64, b: i64) -> i64 {
    return a - b;
}

// TOKENS: Token: KW_FN
// TOKENS: Token: IDENTIFIER Text: 'sub_i64'
// TOKENS: Token: KW_RETURN
// TOKENS: Token: IDENTIFIER Text: 'a'
// TOKENS: Token: OP_MINUS
// TOKENS: Token: IDENTIFIER Text: 'b'

// AST: FuncDecl 'sub_i64'
// AST: BinExpr: 'OP_MINUS'
// AST-NEXT: VarExpr: 'a'
// AST-NEXT: VarExpr: 'b'

// SYMBOLTABLE: ┌─ Global Scope (Depth: 0)
// SYMBOLTABLE: │  sub_i64               : (i64,i64,) -> i64 [Function]

// TYPEDAST: FuncDecl {{.*}} 'sub_i64'
// TYPEDAST: BinExpr {{.*}} 'i64' 'OP_MINUS'
// TYPEDAST: VarExpr {{.*}} 'i64' 'a'
// TYPEDAST: VarExpr {{.*}} 'i64' 'b'

// MLIR: func.func @{{.*}}sub_i64(%arg0: i64, %arg1: i64) -> i64
// MLIR: {{.*}} = arith.subi {{.*}}, {{.*}} : i64
// MLIR: return {{.*}} : i64
