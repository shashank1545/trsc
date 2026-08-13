// RUN: %trsc -dump-token %s | %FileCheck %s --check-prefix=TOKENS
// RUN: %trsc -dump-ast %s | %FileCheck %s --check-prefix=AST
// RUN: %trsc -dump-symboltable %s | %FileCheck %s --check-prefix=SYMBOLTABLE
// RUN: %trsc -dump-typedast %s | %FileCheck %s --check-prefix=TYPEDAST
// RUN: %trsc -optim=raw -emit-mlir %s | %FileCheck %s --check-prefix=MLIR

fn or_bool(a: bool, b: bool) -> bool {
    return a || b;
}

// TOKENS: Token: KW_FN
// TOKENS: Token: IDENTIFIER Text: 'or_bool'
// TOKENS: Token: KW_RETURN
// TOKENS: Token: IDENTIFIER Text: 'a'
// TOKENS: Token: OP_PIPEPIPE
// TOKENS: Token: IDENTIFIER Text: 'b'

// AST: FuncDecl 'or_bool'
// AST: BinExpr: 'OP_PIPEPIPE'
// AST-NEXT: VarExpr: 'a'
// AST-NEXT: VarExpr: 'b'

// SYMBOLTABLE: ┌─ Global Scope (Depth: 0)
// SYMBOLTABLE: │  or_bool           : (bool,bool,) -> bool [Function]

// TYPEDAST: FuncDecl {{.*}} 'or_bool'
// TYPEDAST: BinExpr {{.*}} 'bool' 'OP_PIPEPIPE'
// TYPEDAST: VarExpr {{.*}} 'bool' 'a'
// TYPEDAST: VarExpr {{.*}} 'bool' 'b'

// MLIR: func.func @{{.*}}or_bool(%arg0: i1, %arg1: i1) -> i1
// MLIR: scf.if
// MLIR: scf.yield
// MLIR: scf.yield
// MLIR: return {{.*}} : i1
