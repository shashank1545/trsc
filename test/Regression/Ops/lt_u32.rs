// RUN: %trsc -dump-token %s | %FileCheck %s --check-prefix=TOKENS
// RUN: %trsc -dump-ast %s | %FileCheck %s --check-prefix=AST
// RUN: %trsc -dump-symboltable %s | %FileCheck %s --check-prefix=SYMBOLTABLE
// RUN: %trsc -dump-typedast %s | %FileCheck %s --check-prefix=TYPEDAST
// RUN: %trsc -emit-mlir %s | %FileCheck %s --check-prefix=MLIR

fn lt_u32(a: u32, b: u32) -> bool {
    return a < b;
}

// TOKENS: Token: KW_FN
// TOKENS: Token: IDENTIFIER Text: 'lt_u32'
// TOKENS: Token: KW_RETURN
// TOKENS: Token: IDENTIFIER Text: 'a'
// TOKENS: Token: OP_LESS
// TOKENS: Token: IDENTIFIER Text: 'b'

// AST: FuncDecl 'lt_u32'
// AST: BinExpr: 'OP_LESS'
// AST-NEXT: VarExpr: 'a'
// AST-NEXT: VarExpr: 'b'

// SYMBOLTABLE: ┌─ Global Scope (Depth: 0)
// SYMBOLTABLE: │  lt_u32               : (u32,u32,) -> bool [Function]

// TYPEDAST: FuncDecl {{.*}} 'lt_u32'
// TYPEDAST: BinExpr {{.*}} 'bool' 'OP_LESS'
// TYPEDAST: VarExpr {{.*}} 'u32' 'a'
// TYPEDAST: VarExpr {{.*}} 'u32' 'b'

// MLIR: func.func @{{.*}}lt_u32(%arg0: i32, %arg1: i32) -> i1
// MLIR: {{.*}} = arith.cmpi ult, {{.*}}, {{.*}} : i32
// MLIR: return {{.*}} : i1
