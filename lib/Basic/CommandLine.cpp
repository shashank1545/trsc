#include <cctype>
#include <iostream>
#include <string>

#include "trsc/Basic/CommandLine.h"

namespace trsc {

static bool parseCudaArch(const std::string &value, TargetOptions &target) {
  if (value.size() != 5 || value.rfind("sm_", 0) != 0 ||
      !std::isdigit(static_cast<unsigned char>(value[3])) ||
      !std::isdigit(static_cast<unsigned char>(value[4]))) {
    std::cerr << "Invalid CUDA architecture '" << value
              << "'; expected sm_NN.\n";
    return false;
  }

  int capability = std::stoi(value.substr(3));
  if (capability < 75) {
    std::cerr << "CUDA architecture " << value
              << " is unsupported; minimum is sm_75.\n";
    return false;
  }

  target.CudaArch = value;
  target.MinimumCudaCapability = capability;
  return true;
}

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
    } else if (arg == "-emit-llvm") {
      options.EmitLLVM = true;
    } else if (arg == "-emit-obj") {
      options.EmitObj = true;
    } else if (arg.rfind("-optim=", 0) == 0) {
      std::string val = arg.substr(7);
      if (val == "raw") {
        options.Optim = OptimizationStage::RawMLIR;
      } else if (val == "rawcln") {
        options.Optim = OptimizationStage::CleanedMLIR;
      } else if (val == "loop") {
        options.Optim = OptimizationStage::LoopOptimized;
      } else if (val == "stdlowering") {
        options.Optim = OptimizationStage::StandardLowering;
      } else if (val == "finopt") {
        options.Optim = OptimizationStage::OptimizedMLIR;
      } else {
        std::cerr << "Unrecognized optimization pass.";
        return false;
      }
    } else if (arg.rfind("--matmul-opt-level=", 0) == 0) {
      std::string Val = arg.substr(19);
      try {
        options.MatMulOptLevel = std::stoi(Val);
      } catch (const std::exception &) {
        std::cerr << "Invalid integer for --matmul-opt-level.\n";
        return false;
      }
    } else if (arg.rfind("--device=", 0) == 0) {
      std::string value = arg.substr(9);
      if (value == "auto") {
        options.Target.Device = DeviceMode::Auto;
      } else if (value == "cpu") {
        options.Target.Device = DeviceMode::CPU;
      } else if (value == "cuda") {
        options.Target.Device = DeviceMode::CUDA;
      } else {
        std::cerr << "Invalid --device value '" << value
                  << "'; expected auto, cpu, or cuda.\n";
        return false;
      }
    } else if (arg.rfind("--cuda-arch=", 0) == 0) {
      if (!parseCudaArch(arg.substr(12), options.Target))
        return false;
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
