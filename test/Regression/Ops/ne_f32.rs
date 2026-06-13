// RUN: %trsc -dump-token %s | %FileCheck %s --check-prefix=TOKENS
// RUN: %trsc -dump-ast %s | %FileCheck %s --check-prefix=AST
// RUN: %trsc -dump-symboltable %s | %FileCheck %s --check-prefix=SYMBOLTABLE
// RUN: %trsc -dump-typedast %s | %FileCheck %s --check-prefix=TYPEDAST
// RUN: %trsc -emit-mlir %s | %FileCheck %s --check-prefix=MLIR

fn ne_f32(a: f32, b: f32) -> bool {
    return a != b;
}

// TOKENS: Token: KW_FN
// TOKENS: Token: IDENTIFIER Text: 'ne_f32'
// TOKENS: Token: KW_RETURN
// TOKENS: Token: IDENTIFIER Text: 'a'
// TOKENS: Token: OP_BANGEQUAL
// TOKENS: Token: IDENTIFIER Text: 'b'

// AST: FuncDecl 'ne_f32'
// AST: BinExpr: 'OP_BANGEQUAL'
// AST-NEXT: VarExpr: 'a'
// AST-NEXT: VarExpr: 'b'

// SYMBOLTABLE: ┌─ Global Scope (Depth: 0)
// SYMBOLTABLE: │  ne_f32               : (f32,f32,) -> bool [Function]

// TYPEDAST: FuncDecl {{.*}} 'ne_f32'
// TYPEDAST: BinExpr {{.*}} 'bool' 'OP_BANGEQUAL'
// TYPEDAST: VarExpr {{.*}} 'f32' 'a'
// TYPEDAST: VarExpr {{.*}} 'f32' 'b'

// MLIR: func.func @{{.*}}ne_f32(%arg0: f32, %arg1: f32) -> i1
// MLIR: {{.*}} = arith.cmpf one, {{.*}}, {{.*}} : f32
// MLIR: return {{.*}} : i1
