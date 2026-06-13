// RUN: %trsc -dump-token %s | %FileCheck %s --check-prefix=TOKENS
// RUN: %trsc -dump-ast %s | %FileCheck %s --check-prefix=AST
// RUN: %trsc -dump-symboltable %s | %FileCheck %s --check-prefix=SYMBOLTABLE
// RUN: %trsc -dump-typedast %s | %FileCheck %s --check-prefix=TYPEDAST
// RUN: %trsc -emit-mlir %s | %FileCheck %s --check-prefix=MLIR

fn add_i16(a: i16, b: i16) -> i16 {
    return a + b;
}

// TOKENS: Token: KW_FN
// TOKENS: Token: IDENTIFIER Text: 'add_i16'
// TOKENS: Token: KW_RETURN
// TOKENS: Token: IDENTIFIER Text: 'a'
// TOKENS: Token: OP_PLUS
// TOKENS: Token: IDENTIFIER Text: 'b'

// AST: FuncDecl 'add_i16'
// AST: BinExpr: 'OP_PLUS'
// AST-NEXT: VarExpr: 'a'
// AST-NEXT: VarExpr: 'b'

// SYMBOLTABLE: ┌─ Global Scope (Depth: 0)
// SYMBOLTABLE: │  add_i16               : (i16,i16,) -> i16 [Function]

// TYPEDAST: FuncDecl {{.*}} 'add_i16'
// TYPEDAST: BinExpr {{.*}} 'i16' 'OP_PLUS'
// TYPEDAST: VarExpr {{.*}} 'i16' 'a'
// TYPEDAST: VarExpr {{.*}} 'i16' 'b'

// MLIR: func.func @{{.*}}add_i16(%arg0: i16, %arg1: i16) -> i16
// MLIR: {{.*}} = arith.addi {{.*}}, {{.*}} : i16
// MLIR: return {{.*}} : i16
