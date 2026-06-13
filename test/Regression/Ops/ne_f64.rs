// RUN: %trsc -dump-token %s | %FileCheck %s --check-prefix=TOKENS
// RUN: %trsc -dump-ast %s | %FileCheck %s --check-prefix=AST
// RUN: %trsc -dump-symboltable %s | %FileCheck %s --check-prefix=SYMBOLTABLE
// RUN: %trsc -dump-typedast %s | %FileCheck %s --check-prefix=TYPEDAST
// RUN: %trsc -emit-mlir %s | %FileCheck %s --check-prefix=MLIR

fn ne_f64(a: f64, b: f64) -> bool {
    return a != b;
}

// TOKENS: Token: KW_FN
// TOKENS: Token: IDENTIFIER Text: 'ne_f64'
// TOKENS: Token: KW_RETURN
// TOKENS: Token: IDENTIFIER Text: 'a'
// TOKENS: Token: OP_BANGEQUAL
// TOKENS: Token: IDENTIFIER Text: 'b'

// AST: FuncDecl 'ne_f64'
// AST: BinExpr: 'OP_BANGEQUAL'
// AST-NEXT: VarExpr: 'a'
// AST-NEXT: VarExpr: 'b'

// SYMBOLTABLE: ┌─ Global Scope (Depth: 0)
// SYMBOLTABLE: │  ne_f64               : (f64,f64,) -> bool [Function]

// TYPEDAST: FuncDecl {{.*}} 'ne_f64'
// TYPEDAST: BinExpr {{.*}} 'bool' 'OP_BANGEQUAL'
// TYPEDAST: VarExpr {{.*}} 'f64' 'a'
// TYPEDAST: VarExpr {{.*}} 'f64' 'b'

// MLIR: func.func @{{.*}}ne_f64(%arg0: f64, %arg1: f64) -> i1
// MLIR: {{.*}} = arith.cmpf one, {{.*}}, {{.*}} : f64
// MLIR: return {{.*}} : i1
