// RUN: %trsc -dump-token %s | %FileCheck %s --check-prefix=TOKENS
// RUN: %trsc -dump-ast %s | %FileCheck %s --check-prefix=AST
// RUN: %trsc -dump-symboltable %s | %FileCheck %s --check-prefix=SYMBOLTABLE
// RUN: %trsc -dump-typedast %s | %FileCheck %s --check-prefix=TYPEDAST
// RUN: %trsc -emit-mlir %s | %FileCheck %s --check-prefix=MLIR

fn add_u8(a: u8, b: u8) -> u8 {
    return a + b;
}

// TOKENS: Token: KW_FN
// TOKENS: Token: IDENTIFIER Text: 'add_u8'
// TOKENS: Token: KW_RETURN
// TOKENS: Token: IDENTIFIER Text: 'a'
// TOKENS: Token: OP_PLUS
// TOKENS: Token: IDENTIFIER Text: 'b'

// AST: FuncDecl 'add_u8'
// AST: BinExpr: 'OP_PLUS'
// AST-NEXT: VarExpr: 'a'
// AST-NEXT: VarExpr: 'b'

// SYMBOLTABLE: ┌─ Global Scope (Depth: 0)
// SYMBOLTABLE: │  add_u8               : (u8,u8,) -> u8 [Function]

// TYPEDAST: FuncDecl {{.*}} 'add_u8'
// TYPEDAST: BinExpr {{.*}} 'u8' 'OP_PLUS'
// TYPEDAST: VarExpr {{.*}} 'u8' 'a'
// TYPEDAST: VarExpr {{.*}} 'u8' 'b'

// MLIR: func.func @{{.*}}add_u8(%arg0: i8, %arg1: i8) -> i8
// MLIR: {{.*}} = arith.addi {{.*}}, {{.*}} : i8
// MLIR: return {{.*}} : i8
