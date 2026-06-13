// RUN: %trsc -dump-token %s | %FileCheck %s --check-prefix=TOKENS
// RUN: %trsc -dump-ast %s | %FileCheck %s --check-prefix=AST
// RUN: %trsc -dump-symboltable %s | %FileCheck %s --check-prefix=SYMBOLTABLE
// RUN: %trsc -dump-typedast %s | %FileCheck %s --check-prefix=TYPEDAST
// RUN: %trsc -emit-mlir %s | %FileCheck %s --check-prefix=MLIR

fn ge_i8(a: i8, b: i8) -> bool {
    return a >= b;
}

// TOKENS: Token: KW_FN
// TOKENS: Token: IDENTIFIER Text: 'ge_i8'
// TOKENS: Token: KW_RETURN
// TOKENS: Token: IDENTIFIER Text: 'a'
// TOKENS: Token: OP_GREATEREQUAL
// TOKENS: Token: IDENTIFIER Text: 'b'

// AST: FuncDecl 'ge_i8'
// AST: BinExpr: 'OP_GREATEREQUAL'
// AST-NEXT: VarExpr: 'a'
// AST-NEXT: VarExpr: 'b'

// SYMBOLTABLE: ┌─ Global Scope (Depth: 0)
// SYMBOLTABLE: │  ge_i8               : (i8,i8,) -> bool [Function]

// TYPEDAST: FuncDecl {{.*}} 'ge_i8'
// TYPEDAST: BinExpr {{.*}} 'bool' 'OP_GREATEREQUAL'
// TYPEDAST: VarExpr {{.*}} 'i8' 'a'
// TYPEDAST: VarExpr {{.*}} 'i8' 'b'

// MLIR: func.func @{{.*}}ge_i8(%arg0: i8, %arg1: i8) -> i1
// MLIR: {{.*}} = arith.cmpi sge, {{.*}}, {{.*}} : i8
// MLIR: return {{.*}} : i1
