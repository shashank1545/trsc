// RUN: %trsc -dump-token %s | %FileCheck %s --check-prefix=TOKENS
// RUN: %trsc -dump-ast %s | %FileCheck %s --check-prefix=AST
// RUN: %trsc -dump-symboltable %s | %FileCheck %s --check-prefix=SYMBOLTABLE
// RUN: %trsc -dump-typedast %s | %FileCheck %s --check-prefix=TYPEDAST
// RUN: %trsc -emit-mlir %s | %FileCheck %s --check-prefix=MLIR

fn div_f64(a: f64, b: f64) -> f64 {
    return a / b;
}

// TOKENS: Token: KW_FN
// TOKENS: Token: IDENTIFIER Text: 'div_f64'
// TOKENS: Token: KW_RETURN
// TOKENS: Token: IDENTIFIER Text: 'a'
// TOKENS: Token: OP_SLASH
// TOKENS: Token: IDENTIFIER Text: 'b'

// AST: FuncDecl 'div_f64'
// AST: BinExpr: 'OP_SLASH'
// AST-NEXT: VarExpr: 'a'
// AST-NEXT: VarExpr: 'b'

// SYMBOLTABLE: ┌─ Global Scope (Depth: 0)
// SYMBOLTABLE: │  div_f64               : (f64,f64,) -> f64 [Function]

// TYPEDAST: FuncDecl {{.*}} 'div_f64'
// TYPEDAST: BinExpr {{.*}} 'f64' 'OP_SLASH'
// TYPEDAST: VarExpr {{.*}} 'f64' 'a'
// TYPEDAST: VarExpr {{.*}} 'f64' 'b'

// MLIR: func.func @{{.*}}div_f64(%arg0: f64, %arg1: f64) -> f64
// MLIR: {{.*}} = arith.divf {{.*}}, {{.*}} : f64
// MLIR: return {{.*}} : f64
