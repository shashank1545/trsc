// RUN: %trsc -dump-token %s | %FileCheck %s --check-prefix=TOKENS
// RUN: %trsc -dump-ast %s | %FileCheck %s --check-prefix=AST
// RUN: %trsc -dump-symboltable %s | %FileCheck %s --check-prefix=SYMBOLTABLE
// RUN: %trsc -dump-typedast %s | %FileCheck %s --check-prefix=TYPEDAST
// RUN: %trsc -emit-mlir %s | %FileCheck %s --check-prefix=MLIR

fn mul_i16(a: i16, b: i16) -> i16 {
    return a * b;
}

// TOKENS: Token: KW_FN
// TOKENS: Token: IDENTIFIER Text: 'mul_i16'
// TOKENS: Token: KW_RETURN
// TOKENS: Token: IDENTIFIER Text: 'a'
// TOKENS: Token: OP_STAR
// TOKENS: Token: IDENTIFIER Text: 'b'

// AST: FuncDecl 'mul_i16'
// AST: BinExpr: 'OP_STAR'
// AST-NEXT: VarExpr: 'a'
// AST-NEXT: VarExpr: 'b'

// SYMBOLTABLE: ┌─ Global Scope (Depth: 0)
// SYMBOLTABLE: │  mul_i16               : (i16,i16,) -> i16 [Function]

// TYPEDAST: FuncDecl {{.*}} 'mul_i16'
// TYPEDAST: BinExpr {{.*}} 'i16' 'OP_STAR'
// TYPEDAST: VarExpr {{.*}} 'i16' 'a'
// TYPEDAST: VarExpr {{.*}} 'i16' 'b'

// MLIR: func.func @{{.*}}mul_i16(%arg0: i16, %arg1: i16) -> i16
// MLIR: {{.*}} = arith.muli {{.*}}, {{.*}} : i16
// MLIR: return {{.*}} : i16
