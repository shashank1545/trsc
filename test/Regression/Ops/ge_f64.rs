// RUN: %trsc -dump-token %s | %FileCheck %s --check-prefix=TOKENS
// RUN: %trsc -dump-ast %s | %FileCheck %s --check-prefix=AST
// RUN: %trsc -dump-symboltable %s | %FileCheck %s --check-prefix=SYMBOLTABLE
// RUN: %trsc -dump-typedast %s | %FileCheck %s --check-prefix=TYPEDAST
// RUN: %trsc -emit-mlir %s | %FileCheck %s --check-prefix=MLIR

fn ge_f64(a: f64, b: f64) -> bool {
    return a >= b;
}

// TOKENS: Token: KW_FN
// TOKENS: Token: IDENTIFIER Text: 'ge_f64'
// TOKENS: Token: KW_RETURN
// TOKENS: Token: IDENTIFIER Text: 'a'
// TOKENS: Token: OP_GREATEREQUAL
// TOKENS: Token: IDENTIFIER Text: 'b'

// AST: FuncDecl 'ge_f64'
// AST: BinExpr: 'OP_GREATEREQUAL'
// AST-NEXT: VarExpr: 'a'
// AST-NEXT: VarExpr: 'b'

// SYMBOLTABLE: ┌─ Global Scope (Depth: 0)
// SYMBOLTABLE: │  ge_f64               : (f64,f64,) -> bool [Function]

// TYPEDAST: FuncDecl {{.*}} 'ge_f64'
// TYPEDAST: BinExpr {{.*}} 'bool' 'OP_GREATEREQUAL'
// TYPEDAST: VarExpr {{.*}} 'f64' 'a'
// TYPEDAST: VarExpr {{.*}} 'f64' 'b'

// MLIR: func.func @{{.*}}ge_f64(%arg0: f64, %arg1: f64) -> i1
// MLIR: {{.*}} = arith.cmpf oge, {{.*}}, {{.*}} : f64
// MLIR: return {{.*}} : i1
