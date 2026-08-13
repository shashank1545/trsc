// RUN: %trsc -dump-token %s | %FileCheck %s --check-prefix=TOKENS
// RUN: %trsc -dump-ast %s | %FileCheck %s --check-prefix=AST
// RUN: %trsc -dump-symboltable %s | %FileCheck %s --check-prefix=SYMBOLTABLE
// RUN: %trsc -dump-typedast %s | %FileCheck %s --check-prefix=TYPEDAST
// RUN: %trsc -optim=raw -emit-mlir %s | %FileCheck %s --check-prefix=MLIR

fn and_bool(a: bool, b: bool) -> bool {
    return a && b;
}

// TOKENS: Token: KW_FN
// TOKENS: Token: IDENTIFIER Text: 'and_bool'
// TOKENS: Token: KW_RETURN
// TOKENS: Token: IDENTIFIER Text: 'a'
// TOKENS: Token: OP_AMPAMP
// TOKENS: Token: IDENTIFIER Text: 'b'

// AST: FuncDecl 'and_bool'
// AST: BinExpr: 'OP_AMPAMP'
// AST-NEXT: VarExpr: 'a'
// AST-NEXT: VarExpr: 'b'

// SYMBOLTABLE: ┌─ Global Scope (Depth: 0)
// SYMBOLTABLE: │  and_bool           : (bool,bool,) -> bool [Function]

// TYPEDAST: FuncDecl {{.*}} 'and_bool'
// TYPEDAST: BinExpr {{.*}} 'bool' 'OP_AMPAMP'
// TYPEDAST: VarExpr {{.*}} 'bool' 'a'
// TYPEDAST: VarExpr {{.*}} 'bool' 'b'

// MLIR: func.func @{{.*}}and_bool(%arg0: i1, %arg1: i1) -> i1
// MLIR: scf.if
// MLIR: scf.yield
// MLIR: scf.yield
// MLIR: return {{.*}} : i1
