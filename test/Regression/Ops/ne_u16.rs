// RUN: %trsc -dump-token %s | %FileCheck %s --check-prefix=TOKENS
// RUN: %trsc -dump-ast %s | %FileCheck %s --check-prefix=AST
// RUN: %trsc -dump-symboltable %s | %FileCheck %s --check-prefix=SYMBOLTABLE
// RUN: %trsc -dump-typedast %s | %FileCheck %s --check-prefix=TYPEDAST
// RUN: %trsc -emit-mlir %s | %FileCheck %s --check-prefix=MLIR

fn ne_u16(a: u16, b: u16) -> bool {
    return a != b;
}

// TOKENS: Token: KW_FN
// TOKENS: Token: IDENTIFIER Text: 'ne_u16'
// TOKENS: Token: KW_RETURN
// TOKENS: Token: IDENTIFIER Text: 'a'
// TOKENS: Token: OP_BANGEQUAL
// TOKENS: Token: IDENTIFIER Text: 'b'

// AST: FuncDecl 'ne_u16'
// AST: BinExpr: 'OP_BANGEQUAL'
// AST-NEXT: VarExpr: 'a'
// AST-NEXT: VarExpr: 'b'

// SYMBOLTABLE: ┌─ Global Scope (Depth: 0)
// SYMBOLTABLE: │  ne_u16               : (u16,u16,) -> bool [Function]

// TYPEDAST: FuncDecl {{.*}} 'ne_u16'
// TYPEDAST: BinExpr {{.*}} 'bool' 'OP_BANGEQUAL'
// TYPEDAST: VarExpr {{.*}} 'u16' 'a'
// TYPEDAST: VarExpr {{.*}} 'u16' 'b'

// MLIR: func.func @{{.*}}ne_u16(%arg0: i16, %arg1: i16) -> i1
// MLIR: {{.*}} = arith.cmpi ne, {{.*}}, {{.*}} : i16
// MLIR: return {{.*}} : i1
