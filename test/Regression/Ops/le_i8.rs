// RUN: %trsc -dump-token %s | %FileCheck %s --check-prefix=TOKENS
// RUN: %trsc -dump-ast %s | %FileCheck %s --check-prefix=AST
// RUN: %trsc -dump-symboltable %s | %FileCheck %s --check-prefix=SYMBOLTABLE
// RUN: %trsc -dump-typedast %s | %FileCheck %s --check-prefix=TYPEDAST
// RUN: %trsc -emit-mlir %s | %FileCheck %s --check-prefix=MLIR

fn le_i8(a: i8, b: i8) -> bool {
    return a <= b;
}

// TOKENS: Token: KW_FN
// TOKENS: Token: IDENTIFIER Text: 'le_i8'
// TOKENS: Token: KW_RETURN
// TOKENS: Token: IDENTIFIER Text: 'a'
// TOKENS: Token: OP_LESSEQUAL
// TOKENS: Token: IDENTIFIER Text: 'b'

// AST: FuncDecl 'le_i8'
// AST: BinExpr: 'OP_LESSEQUAL'
// AST-NEXT: VarExpr: 'a'
// AST-NEXT: VarExpr: 'b'

// SYMBOLTABLE: ┌─ Global Scope (Depth: 0)
// SYMBOLTABLE: │  le_i8               : (i8,i8,) -> bool [Function]

// TYPEDAST: FuncDecl {{.*}} 'le_i8'
// TYPEDAST: BinExpr {{.*}} 'bool' 'OP_LESSEQUAL'
// TYPEDAST: VarExpr {{.*}} 'i8' 'a'
// TYPEDAST: VarExpr {{.*}} 'i8' 'b'

// MLIR: func.func @{{.*}}le_i8(%arg0: i8, %arg1: i8) -> i1
// MLIR: {{.*}} = arith.cmpi sle, {{.*}}, {{.*}} : i8
// MLIR: return {{.*}} : i1
