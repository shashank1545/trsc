// RUN: %trsc -dump-token %s | %FileCheck %s --check-prefix=TOKENS
// RUN: %trsc -dump-ast %s | %FileCheck %s --check-prefix=AST
// RUN: %trsc -dump-symboltable %s | %FileCheck %s --check-prefix=SYMBOLTABLE
// RUN: %trsc -dump-typedast %s | %FileCheck %s --check-prefix=TYPEDAST
// RUN: %trsc -emit-mlir %s | %FileCheck %s --check-prefix=MLIR

fn mul_i8(a: i8, b: i8) -> i8 {
    return a * b;
}

// TOKENS: Token: KW_FN
// TOKENS: Token: IDENTIFIER Text: 'mul_i8'
// TOKENS: Token: KW_RETURN
// TOKENS: Token: IDENTIFIER Text: 'a'
// TOKENS: Token: OP_STAR
// TOKENS: Token: IDENTIFIER Text: 'b'

// AST: FuncDecl 'mul_i8'
// AST: BinExpr: 'OP_STAR'
// AST-NEXT: VarExpr: 'a'
// AST-NEXT: VarExpr: 'b'

// SYMBOLTABLE: ┌─ Global Scope (Depth: 0)
// SYMBOLTABLE: │  mul_i8               : (i8,i8,) -> i8 [Function]

// TYPEDAST: FuncDecl {{.*}} 'mul_i8'
// TYPEDAST: BinExpr {{.*}} 'i8' 'OP_STAR'
// TYPEDAST: VarExpr {{.*}} 'i8' 'a'
// TYPEDAST: VarExpr {{.*}} 'i8' 'b'

// MLIR: func.func @{{.*}}mul_i8(%arg0: i8, %arg1: i8) -> i8
// MLIR: {{.*}} = arith.muli {{.*}}, {{.*}} : i8
// MLIR: return {{.*}} : i8
