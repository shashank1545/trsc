// RUN: %trsc -dump-token %s | %FileCheck %s --check-prefix=TOKENS
// RUN: %trsc -dump-ast %s | %FileCheck %s --check-prefix=AST
// RUN: %trsc -dump-symboltable %s | %FileCheck %s --check-prefix=SYMBOLTABLE
// RUN: %trsc -dump-typedast %s | %FileCheck %s --check-prefix=TYPEDAST
// RUN: %trsc -emit-mlir %s | %FileCheck %s --check-prefix=MLIR

fn le_i32(a: i32, b: i32) -> bool {
    return a <= b;
}

// TOKENS: Token: KW_FN
// TOKENS: Token: IDENTIFIER Text: 'le_i32'
// TOKENS: Token: KW_RETURN
// TOKENS: Token: IDENTIFIER Text: 'a'
// TOKENS: Token: OP_LESSEQUAL
// TOKENS: Token: IDENTIFIER Text: 'b'

// AST: FuncDecl 'le_i32'
// AST: BinExpr: 'OP_LESSEQUAL'
// AST-NEXT: VarExpr: 'a'
// AST-NEXT: VarExpr: 'b'

// SYMBOLTABLE: ┌─ Global Scope (Depth: 0)
// SYMBOLTABLE: │  le_i32               : (i32,i32,) -> bool [Function]

// TYPEDAST: FuncDecl {{.*}} 'le_i32'
// TYPEDAST: BinExpr {{.*}} 'bool' 'OP_LESSEQUAL'
// TYPEDAST: VarExpr {{.*}} 'i32' 'a'
// TYPEDAST: VarExpr {{.*}} 'i32' 'b'

// MLIR: func.func @{{.*}}le_i32(%arg0: i32, %arg1: i32) -> i1
// MLIR: {{.*}} = arith.cmpi sle, {{.*}}, {{.*}} : i32
// MLIR: return {{.*}} : i1
