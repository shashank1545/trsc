// RUN: %trsc -dump-token %s | %FileCheck %s --check-prefix=TOKENS
// RUN: %trsc -dump-ast %s | %FileCheck %s --check-prefix=AST
// RUN: %trsc -dump-symboltable %s | %FileCheck %s --check-prefix=SYMBOLTABLE
// RUN: %trsc -dump-typedast %s | %FileCheck %s --check-prefix=TYPEDAST
// RUN: %trsc -emit-mlir %s | %FileCheck %s --check-prefix=MLIR

fn eq_f32(a: f32, b: f32) -> bool {
    return a == b;
}

// TOKENS: Token: KW_FN
// TOKENS: Token: IDENTIFIER Text: 'eq_f32'
// TOKENS: Token: KW_RETURN
// TOKENS: Token: IDENTIFIER Text: 'a'
// TOKENS: Token: OP_EQUALEQUAL
// TOKENS: Token: IDENTIFIER Text: 'b'

// AST: FuncDecl 'eq_f32'
// AST: BinExpr: 'OP_EQUALEQUAL'
// AST-NEXT: VarExpr: 'a'
// AST-NEXT: VarExpr: 'b'

// SYMBOLTABLE: ┌─ Global Scope (Depth: 0)
// SYMBOLTABLE: │  eq_f32               : (f32,f32,) -> bool [Function]

// TYPEDAST: FuncDecl {{.*}} 'eq_f32'
// TYPEDAST: BinExpr {{.*}} 'bool' 'OP_EQUALEQUAL'
// TYPEDAST: VarExpr {{.*}} 'f32' 'a'
// TYPEDAST: VarExpr {{.*}} 'f32' 'b'

// MLIR: func.func @{{.*}}eq_f32(%arg0: f32, %arg1: f32) -> i1
// MLIR: {{.*}} = arith.cmpf oeq, {{.*}}, {{.*}} : f32
// MLIR: return {{.*}} : i1
