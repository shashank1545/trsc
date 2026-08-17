#ifndef TRSC_BASIC_COMMANDLINE_H
#define TRSC_BASIC_COMMANDLINE_H

#include <string>

#include "trsc/Basic/TargetOptions.h"

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
  bool ShowHelp = false;
  bool ShowVersion = false;
  bool DumpSymbol = false;
  bool DumpSymbolTable = false;
  bool DumpTypedAST = false;
  bool EmitMLIR = false;
  bool EmitLLVM = false;
  bool EmitObj = false;
  int MatMulOptLevel = 6;
  TargetOptions Target;
  OptimizationStage Optim = OptimizationStage::OptimizedMLIR;
};

bool parseCommandLine(int Argc, char **Argv, CompilerOptions &Options);

void printHelp(const char *ProgramName);
void printVersion();

} // namespace trsc

#endif // TRSC_BASIC_COMMANDLINE_H
