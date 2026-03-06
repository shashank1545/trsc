#include <iostream>
#include <string>

#include "trsc/Basic/CommandLine.h"

namespace trsc {

bool parseCommandLine(int argc, char **argv, CompilerOptions &options) {
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "-dump-token") {
      options.DumpLexerTokens = true;
    } else if (arg == "-dump-ast") {
      options.DumpAST = true;
    } else if (arg == "-v" || arg == "--verbose") {
      options.Verbose = true;
    } else if (arg == "-dump-symbol") {
      options.DumpSymbol = true;
    } else if (arg == "-dump-symboltable") {
      options.DumpSymbolTable = true;
    } else if (arg == "-dump-typedast") {
      options.DumpTypedAST = true;
    } else if (arg == "-emit-mlir") {
      options.EmitMLIR = true;
    } else if (arg == "-o") {
      if (i + 1 < argc) {
        options.OutputFile = argv[++i];
      } else {
        std::cerr << "Error: -o requires a filename." << '\n';
        return false;
      }
    } else {
      if (!options.InputFile.empty()) {
        std::cerr << "Error: Only one input file can be specified." << '\n';
        return false;
      }
      options.InputFile = arg;
    }
  }

  if (options.InputFile.empty()) {
    std::cerr << "Error: No input file specified." << '\n';
    return false;
  }

  return true;
}

} // namespace trsc

