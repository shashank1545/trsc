// RUN: %trsc -dump-token %s | %FileCheck %s --check-prefix=TOKENS
// RUN: %trsc -dump-ast %s | %FileCheck %s --check-prefix=AST
// RUN: %trsc -dump-symboltable %s | %FileCheck %s --check-prefix=SYMBOLTABLE
// RUN: %trsc -dump-typedast %s | %FileCheck %s --check-prefix=TYPEDAST
// RUN: %trsc -emit-mlir %s | %FileCheck %s --check-prefix=MLIR

fn add_u16(a: u16, b: u16) -> u16 {
    return a + b;
}

// TOKENS: Token: KW_FN
// TOKENS: Token: IDENTIFIER Text: 'add_u16'
// TOKENS: Token: KW_RETURN
// TOKENS: Token: IDENTIFIER Text: 'a'
// TOKENS: Token: OP_PLUS
// TOKENS: Token: IDENTIFIER Text: 'b'

// AST: FuncDecl 'add_u16'
// AST: BinExpr: 'OP_PLUS'
// AST-NEXT: VarExpr: 'a'
// AST-NEXT: VarExpr: 'b'

// SYMBOLTABLE: ┌─ Global Scope (Depth: 0)
// SYMBOLTABLE: │  add_u16               : (u16,u16,) -> u16 [Function]

// TYPEDAST: FuncDecl {{.*}} 'add_u16'
// TYPEDAST: BinExpr {{.*}} 'u16' 'OP_PLUS'
// TYPEDAST: VarExpr {{.*}} 'u16' 'a'
// TYPEDAST: VarExpr {{.*}} 'u16' 'b'

// MLIR: func.func @{{.*}}add_u16(%arg0: i16, %arg1: i16) -> i16
// MLIR: {{.*}} = arith.addi {{.*}}, {{.*}} : i16
// MLIR: return {{.*}} : i16
