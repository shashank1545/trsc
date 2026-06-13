// RUN: %trsc -dump-token %s | %FileCheck %s --check-prefix=TOKENS
// RUN: %trsc -dump-ast %s | %FileCheck %s --check-prefix=AST
// RUN: %trsc -dump-symboltable %s | %FileCheck %s --check-prefix=SYMBOLTABLE
// RUN: %trsc -dump-typedast %s | %FileCheck %s --check-prefix=TYPEDAST
// RUN: %trsc -emit-mlir %s | %FileCheck %s --check-prefix=MLIR

fn le_i16(a: i16, b: i16) -> bool {
    return a <= b;
}

// TOKENS: Token: KW_FN
// TOKENS: Token: IDENTIFIER Text: 'le_i16'
// TOKENS: Token: KW_RETURN
// TOKENS: Token: IDENTIFIER Text: 'a'
// TOKENS: Token: OP_LESSEQUAL
// TOKENS: Token: IDENTIFIER Text: 'b'

// AST: FuncDecl 'le_i16'
// AST: BinExpr: 'OP_LESSEQUAL'
// AST-NEXT: VarExpr: 'a'
// AST-NEXT: VarExpr: 'b'

// SYMBOLTABLE: ┌─ Global Scope (Depth: 0)
// SYMBOLTABLE: │  le_i16               : (i16,i16,) -> bool [Function]

// TYPEDAST: FuncDecl {{.*}} 'le_i16'
// TYPEDAST: BinExpr {{.*}} 'bool' 'OP_LESSEQUAL'
// TYPEDAST: VarExpr {{.*}} 'i16' 'a'
// TYPEDAST: VarExpr {{.*}} 'i16' 'b'

// MLIR: func.func @{{.*}}le_i16(%arg0: i16, %arg1: i16) -> i1
// MLIR: {{.*}} = arith.cmpi sle, {{.*}}, {{.*}} : i16
// MLIR: return {{.*}} : i1
