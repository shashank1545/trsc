// RUN: %trsc -dump-token %s | %FileCheck %s --check-prefix=TOKENS
// RUN: %trsc -dump-ast %s | %FileCheck %s --check-prefix=AST
// RUN: %trsc -dump-symboltable %s | %FileCheck %s --check-prefix=SYMBOLTABLE
// RUN: %trsc -dump-typedast %s | %FileCheck %s --check-prefix=TYPEDAST
// RUN: %trsc -emit-mlir %s | %FileCheck %s --check-prefix=MLIR

fn sub_f32(a: f32, b: f32) -> f32 {
    return a - b;
}

// TOKENS: Token: KW_FN
// TOKENS: Token: IDENTIFIER Text: 'sub_f32'
// TOKENS: Token: KW_RETURN
// TOKENS: Token: IDENTIFIER Text: 'a'
// TOKENS: Token: OP_MINUS
// TOKENS: Token: IDENTIFIER Text: 'b'

// AST: FuncDecl 'sub_f32'
// AST: BinExpr: 'OP_MINUS'
// AST-NEXT: VarExpr: 'a'
// AST-NEXT: VarExpr: 'b'

// SYMBOLTABLE: ┌─ Global Scope (Depth: 0)
// SYMBOLTABLE: │  sub_f32               : (f32,f32,) -> f32 [Function]

// TYPEDAST: FuncDecl {{.*}} 'sub_f32'
// TYPEDAST: BinExpr {{.*}} 'f32' 'OP_MINUS'
// TYPEDAST: VarExpr {{.*}} 'f32' 'a'
// TYPEDAST: VarExpr {{.*}} 'f32' 'b'

// MLIR: func.func @{{.*}}sub_f32(%arg0: f32, %arg1: f32) -> f32
// MLIR: {{.*}} = arith.subf {{.*}}, {{.*}} : f32
// MLIR: return {{.*}} : f32
