// RUN: %trsc -dump-token %s | %FileCheck %s --check-prefix=TOKENS
// RUN: %trsc -dump-ast %s | %FileCheck %s --check-prefix=AST
// RUN: %trsc -dump-symboltable %s | %FileCheck %s --check-prefix=SYMBOLTABLE
// RUN: %trsc -dump-typedast %s | %FileCheck %s --check-prefix=TYPEDAST
// RUN: %trsc -emit-mlir %s | %FileCheck %s --check-prefix=MLIR

fn ne_u64(a: u64, b: u64) -> bool {
    return a != b;
}

// TOKENS: Token: KW_FN
// TOKENS: Token: IDENTIFIER Text: 'ne_u64'
// TOKENS: Token: KW_RETURN
// TOKENS: Token: IDENTIFIER Text: 'a'
// TOKENS: Token: OP_BANGEQUAL
// TOKENS: Token: IDENTIFIER Text: 'b'

// AST: FuncDecl 'ne_u64'
// AST: BinExpr: 'OP_BANGEQUAL'
// AST-NEXT: VarExpr: 'a'
// AST-NEXT: VarExpr: 'b'

// SYMBOLTABLE: ┌─ Global Scope (Depth: 0)
// SYMBOLTABLE: │  ne_u64               : (u64,u64,) -> bool [Function]

// TYPEDAST: FuncDecl {{.*}} 'ne_u64'
// TYPEDAST: BinExpr {{.*}} 'bool' 'OP_BANGEQUAL'
// TYPEDAST: VarExpr {{.*}} 'u64' 'a'
// TYPEDAST: VarExpr {{.*}} 'u64' 'b'

// MLIR: func.func @{{.*}}ne_u64(%arg0: i64, %arg1: i64) -> i1
// MLIR: {{.*}} = arith.cmpi ne, {{.*}}, {{.*}} : i64
// MLIR: return {{.*}} : i1
