#ifndef TRSC_BASIC_COMMANDLINE_H
#define TRSC_BASIC_COMMANDLINE_H

#include <string>

namespace trsc {

enum class OptimizationStage {
  RawMLIR,
  CleanedMLIR,
  LoopOptimized,
  StandardLowering,
  OptimizedMLIR,
};

struct CompilerOptions {
  std::string InputFile;
  std::string OutputFile;
  bool DumpLexerTokens = false;
  bool DumpAST = false;
  bool Verbose = false;
  bool DumpSymbol = false;
  bool DumpSymbolTable = false;
  bool DumpTypedAST = false;
  bool EmitMLIR = false;
  int MatMulOptLevel = 6;
  OptimizationStage Optim = OptimizationStage::OptimizedMLIR;
};

bool parseCommandLine(int Argc, char **Argv, CompilerOptions &Options);

} // namespace trsc

#endif // TRSC_BASIC_COMMANDLINE_H
