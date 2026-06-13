// RUN: %trsc -dump-token %s | %FileCheck %s --check-prefix=TOKENS
// RUN: %trsc -dump-ast %s | %FileCheck %s --check-prefix=AST
// RUN: %trsc -dump-symboltable %s | %FileCheck %s --check-prefix=SYMBOLTABLE
// RUN: %trsc -dump-typedast %s | %FileCheck %s --check-prefix=TYPEDAST
// RUN: %trsc -emit-mlir %s | %FileCheck %s --check-prefix=MLIR

fn ge_i64(a: i64, b: i64) -> bool {
    return a >= b;
}

// TOKENS: Token: KW_FN
// TOKENS: Token: IDENTIFIER Text: 'ge_i64'
// TOKENS: Token: KW_RETURN
// TOKENS: Token: IDENTIFIER Text: 'a'
// TOKENS: Token: OP_GREATEREQUAL
// TOKENS: Token: IDENTIFIER Text: 'b'

// AST: FuncDecl 'ge_i64'
// AST: BinExpr: 'OP_GREATEREQUAL'
// AST-NEXT: VarExpr: 'a'
// AST-NEXT: VarExpr: 'b'

// SYMBOLTABLE: ┌─ Global Scope (Depth: 0)
// SYMBOLTABLE: │  ge_i64               : (i64,i64,) -> bool [Function]

// TYPEDAST: FuncDecl {{.*}} 'ge_i64'
// TYPEDAST: BinExpr {{.*}} 'bool' 'OP_GREATEREQUAL'
// TYPEDAST: VarExpr {{.*}} 'i64' 'a'
// TYPEDAST: VarExpr {{.*}} 'i64' 'b'

// MLIR: func.func @{{.*}}ge_i64(%arg0: i64, %arg1: i64) -> i1
// MLIR: {{.*}} = arith.cmpi sge, {{.*}}, {{.*}} : i64
// MLIR: return {{.*}} : i1
