// RUN: %trsc -dump-tokens %s | %FileCheck %s --check-prefix=TOKENS
// RUN: %trsc -dump-ast %s | %FileCheck %s --check-prefix=AST
// RUN: %trsc -dump-symboltable %s | %FileCheck %s --check-prefix=SYMBOL
// RUN: %trsc -emit-mlir %s | %FileCheck %s --check-prefix=MLIR

fn add_i8(a: i8, b: i8) -> i8 {
    return a + b;
}

// TOKENS: kw_fn
// TOKENS-NEXT: identifier: add_i8
// TOKENS: kw_return
// TOKENS: identifier: a
// TOKENS: plus
// TOKENS: identifier: b

// AST: FunctionDecl: add_i8
// AST: BinaryOperator: +
// AST-NEXT: DeclRefExpr: a
// AST-NEXT: DeclRefExpr: b

// SYMBOL: Symbol: a (i8)
// SYMBOL: Symbol: b (i8)

// MLIR: func @add_i8(%arg0: i8, %arg1: i8) -> i8
// MLIR: %0 = trsc.add %arg0, %arg1 : i8
// MLIR: return %0 : i8
