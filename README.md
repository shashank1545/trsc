# trsc: A Tiny Rust Subset Compiler

`trsc` is a compiler for a subset of the Rust language, built using C++17, LLVM, and MLIR. It is designed to demonstrate modern compiler architecture, including a custom AST, semantic analysis (borrow checking, type checking), and MLIR-based code generation.

## 🚀 Features

- **Lexer & Parser:** Hand-written recursive descent parser for a Rust-like syntax.
- **Semantic Analysis:**
  - **Symbol Table:** Nested scope management and shadowing support.
  - **Type Checker:** Strong static typing for primitive types and functions.
  - **Borrow Checker:** Initial support for memory safety and ownership rules.
- **MLIR Generation:** Lowers high-level AST constructs into a basic MLIR dialect for further optimization and lowering to LLVM IR.
- **Debugging Tools:** Comprehensive dumping flags for every compiler phase.

## Building the Project

### Prerequisites
- **LLVM & MLIR:** Must be installed and discoverable via CMake (`LLVM_DIR` and `MLIR_DIR`).
- **Clang:** Used as the default C++ compiler.
- **Python:** Required for running `lit` tests.

### Build Steps
```bash
mkdir build && cd build
cmake .. -DLLVM_DIR=/path/to/llvm/lib/cmake/llvm -DMLIR_DIR=/path/to/llvm/lib/cmake/mlir
make 
```

## Testing Strategy

`trsc` uses a dual-testing approach for maximum reliability. See [test.md](test.md) for more details.

### 1. Unit Testing (Google Test)
Used for testing individual C++ components like the Lexer and Symbol Table.
```bash
make trsc_unit_tests
./test/trsc_unit_tests
```

### 2. Regression Testing (Lit + FileCheck)
Used for end-to-end verification of compiler output (AST, MLIR, etc.).
```bash
make check-trsc
# or
lit -v ../test
```

## Usage

Run the `trsc` compiler with various flags to inspect different phases:

```bash
# Dump the AST
./bin/trsc --dump-ast example.rs

# Dump TypedAST (this is the final ast after the sema containing type and scope info)
./bin/trsc --dump-typedast example.rs

# Emit MLIR
./bin/trsc --emit-mlir example.rs

# Inspect the Symbol Table
./bin/trsc --dump-symboltable example.rs

# Tokenize only
./bin/trsc --dump-tokens example.rs
```

