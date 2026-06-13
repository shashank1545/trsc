// RUN: %trsc -dump-token %s | %FileCheck %s --check-prefix=TOKENS
// RUN: %trsc -dump-ast %s | %FileCheck %s --check-prefix=AST
// RUN: %trsc -dump-symboltable %s | %FileCheck %s --check-prefix=SYMBOLTABLE
// RUN: %trsc -dump-typedast %s | %FileCheck %s --check-prefix=TYPEDAST
// RUN: %trsc -emit-mlir %s | %FileCheck %s --check-prefix=MLIR

fn le_u8(a: u8, b: u8) -> bool {
    return a <= b;
}

// TOKENS: Token: KW_FN
// TOKENS: Token: IDENTIFIER Text: 'le_u8'
// TOKENS: Token: KW_RETURN
// TOKENS: Token: IDENTIFIER Text: 'a'
// TOKENS: Token: OP_LESSEQUAL
// TOKENS: Token: IDENTIFIER Text: 'b'

// AST: FuncDecl 'le_u8'
// AST: BinExpr: 'OP_LESSEQUAL'
// AST-NEXT: VarExpr: 'a'
// AST-NEXT: VarExpr: 'b'

// SYMBOLTABLE: ┌─ Global Scope (Depth: 0)
// SYMBOLTABLE: │  le_u8               : (u8,u8,) -> bool [Function]

// TYPEDAST: FuncDecl {{.*}} 'le_u8'
// TYPEDAST: BinExpr {{.*}} 'bool' 'OP_LESSEQUAL'
// TYPEDAST: VarExpr {{.*}} 'u8' 'a'
// TYPEDAST: VarExpr {{.*}} 'u8' 'b'

// MLIR: func.func @{{.*}}le_u8(%arg0: i8, %arg1: i8) -> i1
// MLIR: {{.*}} = arith.cmpi ule, {{.*}}, {{.*}} : i8
// MLIR: return {{.*}} : i1
