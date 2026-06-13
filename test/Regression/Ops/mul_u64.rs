// RUN: %trsc -dump-token %s | %FileCheck %s --check-prefix=TOKENS
// RUN: %trsc -dump-ast %s | %FileCheck %s --check-prefix=AST
// RUN: %trsc -dump-symboltable %s | %FileCheck %s --check-prefix=SYMBOLTABLE
// RUN: %trsc -dump-typedast %s | %FileCheck %s --check-prefix=TYPEDAST
// RUN: %trsc -emit-mlir %s | %FileCheck %s --check-prefix=MLIR

fn mul_u64(a: u64, b: u64) -> u64 {
    return a * b;
}

// TOKENS: Token: KW_FN
// TOKENS: Token: IDENTIFIER Text: 'mul_u64'
// TOKENS: Token: KW_RETURN
// TOKENS: Token: IDENTIFIER Text: 'a'
// TOKENS: Token: OP_STAR
// TOKENS: Token: IDENTIFIER Text: 'b'

// AST: FuncDecl 'mul_u64'
// AST: BinExpr: 'OP_STAR'
// AST-NEXT: VarExpr: 'a'
// AST-NEXT: VarExpr: 'b'

// SYMBOLTABLE: ┌─ Global Scope (Depth: 0)
// SYMBOLTABLE: │  mul_u64               : (u64,u64,) -> u64 [Function]

// TYPEDAST: FuncDecl {{.*}} 'mul_u64'
// TYPEDAST: BinExpr {{.*}} 'u64' 'OP_STAR'
// TYPEDAST: VarExpr {{.*}} 'u64' 'a'
// TYPEDAST: VarExpr {{.*}} 'u64' 'b'

// MLIR: func.func @{{.*}}mul_u64(%arg0: i64, %arg1: i64) -> i64
// MLIR: {{.*}} = arith.muli {{.*}}, {{.*}} : i64
// MLIR: return {{.*}} : i64
