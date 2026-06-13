// RUN: %trsc -dump-token %s | %FileCheck %s --check-prefix=TOKENS
// RUN: %trsc -dump-ast %s | %FileCheck %s --check-prefix=AST
// RUN: %trsc -dump-symboltable %s | %FileCheck %s --check-prefix=SYMBOLTABLE
// RUN: %trsc -dump-typedast %s | %FileCheck %s --check-prefix=TYPEDAST
// RUN: %trsc -emit-mlir %s | %FileCheck %s --check-prefix=MLIR

fn eq_u32(a: u32, b: u32) -> bool {
    return a == b;
}

// TOKENS: Token: KW_FN
// TOKENS: Token: IDENTIFIER Text: 'eq_u32'
// TOKENS: Token: KW_RETURN
// TOKENS: Token: IDENTIFIER Text: 'a'
// TOKENS: Token: OP_EQUALEQUAL
// TOKENS: Token: IDENTIFIER Text: 'b'

// AST: FuncDecl 'eq_u32'
// AST: BinExpr: 'OP_EQUALEQUAL'
// AST-NEXT: VarExpr: 'a'
// AST-NEXT: VarExpr: 'b'

// SYMBOLTABLE: ┌─ Global Scope (Depth: 0)
// SYMBOLTABLE: │  eq_u32               : (u32,u32,) -> bool [Function]

// TYPEDAST: FuncDecl {{.*}} 'eq_u32'
// TYPEDAST: BinExpr {{.*}} 'bool' 'OP_EQUALEQUAL'
// TYPEDAST: VarExpr {{.*}} 'u32' 'a'
// TYPEDAST: VarExpr {{.*}} 'u32' 'b'

// MLIR: func.func @{{.*}}eq_u32(%arg0: i32, %arg1: i32) -> i1
// MLIR: {{.*}} = arith.cmpi eq, {{.*}}, {{.*}} : i32
// MLIR: return {{.*}} : i1
