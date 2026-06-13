// RUN: %trsc -dump-token %s | %FileCheck %s --check-prefix=TOKENS
// RUN: %trsc -dump-ast %s | %FileCheck %s --check-prefix=AST
// RUN: %trsc -dump-symboltable %s | %FileCheck %s --check-prefix=SYMBOLTABLE
// RUN: %trsc -dump-typedast %s | %FileCheck %s --check-prefix=TYPEDAST
// RUN: %trsc -emit-mlir %s | %FileCheck %s --check-prefix=MLIR

fn lt_f32(a: f32, b: f32) -> bool {
    return a < b;
}

// TOKENS: Token: KW_FN
// TOKENS: Token: IDENTIFIER Text: 'lt_f32'
// TOKENS: Token: KW_RETURN
// TOKENS: Token: IDENTIFIER Text: 'a'
// TOKENS: Token: OP_LESS
// TOKENS: Token: IDENTIFIER Text: 'b'

// AST: FuncDecl 'lt_f32'
// AST: BinExpr: 'OP_LESS'
// AST-NEXT: VarExpr: 'a'
// AST-NEXT: VarExpr: 'b'

// SYMBOLTABLE: ┌─ Global Scope (Depth: 0)
// SYMBOLTABLE: │  lt_f32               : (f32,f32,) -> bool [Function]

// TYPEDAST: FuncDecl {{.*}} 'lt_f32'
// TYPEDAST: BinExpr {{.*}} 'bool' 'OP_LESS'
// TYPEDAST: VarExpr {{.*}} 'f32' 'a'
// TYPEDAST: VarExpr {{.*}} 'f32' 'b'

// MLIR: func.func @{{.*}}lt_f32(%arg0: f32, %arg1: f32) -> i1
// MLIR: {{.*}} = arith.cmpf olt, {{.*}}, {{.*}} : f32
// MLIR: return {{.*}} : i1
