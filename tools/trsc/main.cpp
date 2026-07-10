#include <fstream>

#include "mlir/Pass/PassManager.h"
#include "mlir/Support/LLVM.h"
#include "mlir/Target/LLVMIR/Dialect/All.h"
#include "mlir/Target/LLVMIR/Export.h"

#include "llvm/IR/Module.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/CodeGen.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/Program.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/TargetParser/Host.h"

#include "trsc/Lex/Lexer.h"
#include "trsc/Parse/Parser.h"
#include "trsc/AST/ASTPrinter.h"
#include "trsc/AST/TypedASTPrinter.h"
#include "trsc/AST/ASTContext.h"
#include "trsc/Basic/CommandLine.h"
#include "trsc/Sema/Sema.h" 
#include "trsc/Sema/SymbolTablePrinter.h"
#include "trsc/MLIR/TrscMLIRGen.h"
#include "trsc/MLIR/Transforms/PassPipeline.h"
#include "trsc/MLIR/MatMulOpts/MatMulOptPasses.h"

#include "llvm/Support/FileSystem.h"

int main(int argc, char **argv) {
  trsc::CompilerOptions options;
  if (!trsc::parseCommandLine(argc, argv, options)) {
    return 1;
  }

  trsc::DiagnosticsEngine Diag;
  trsc::SourceManager SM(Diag);

  if (options.Verbose) {
    std::cerr << "Starting compilation for: " << options.InputFile << "\n";
  }

  if (!SM.loadFile(options.InputFile)) {
    return 1;
  }

  if (options.Verbose) {
    std::cerr << "Starting Lexical Analysis..." << "\n";
  }
  trsc::Lex::Lexer Lex(SM, Diag);
  std::vector<trsc::Lex::Token> Tokens;
  trsc::Lex::Token Tok;
  do {
    Tok = Lex.Lex();
    if (options.DumpLexerTokens) {
      std::cout << "Token: " << trsc::Lex::getTokenName(Tok.getKind())
        << " Text: '" << Tok.getText() << "'"
        << " Location: " << Tok.getLocation().Line << ":"
        << Tok.getLocation().Column << "\n";
    }
    Tokens.push_back(Tok);
  } while (Tok.getKind() != trsc::Lex::TokenKind::ENDOFFILE);

  if (Diag.getNumErrors() > 0) {
    std::cerr << "Lexing failed with " << Diag.getNumErrors() << " errors."
      << "\n";
    return 1;
  }
  if (options.Verbose) {
    std::cerr << "Lexical Analysis complete. " << Tokens.size() << " tokens found." << "\n";
  }

  if (options.DumpLexerTokens) {
    if (options.Verbose) {
      std::cerr << "Exiting after Lexical Analysis (dump-tokens requested)." << "\n";
    }
    return 0;
  }

  if (options.Verbose) {
    std::cerr << "Starting Parsing..." << "\n";
  }

  trsc::Parser Parser(Diag, Tokens);
  std::unique_ptr<trsc::Program> AST = Parser.parse();

  if (Diag.getNumErrors() > 0) {
    std::cerr << "Parsing failed with " << Diag.getNumErrors() << " errors."
      << "\n";
    return 1;
  }
  if (options.Verbose) {
    std::cerr << "Parsing complete." << "\n";
  }

  if (options.DumpAST) {
    if (AST) { 
      if (!options.OutputFile.empty()) {
        std::ofstream outfile(options.OutputFile);
        if (!outfile) {
          std::cerr << "Error: Could not open output file: " << options.OutputFile << "\n";
          return 1;
        }
        trsc::ASTPrinter printer(outfile);
        printer.visit(AST.get());
      } else {
        trsc::ASTPrinter printer(std::cout);
        printer.visit(AST.get());
      }
    }
    if (options.Verbose) {
      std::cerr << "Exiting after Parsing (dump-ast requested)." << "\n";
    }
    return 0;
  }

  if (options.Verbose) {
    std::cerr << "Starting Semantic Analysis..." << "\n";
  }

  trsc::SymbolTable ST;
  trsc::ASTContext Ctx;
  trsc::SemanticAnalyzer Sema(Diag, ST, Ctx);
  Sema.analyze(AST.get());

  if (Diag.getNumErrors() > 0) {
    std::cerr << "Semantic analysis failed with " << Diag.getNumErrors() << " errors."
      << "\n";
    return 1;
  }
  if (options.Verbose) {
    std::cerr << "Semantic Analysis complete." << "\n";
  }
  if(options.DumpSymbol) {
    if (!options.OutputFile.empty()) {
      std::ofstream outfile(options.OutputFile);
      if (!outfile) {
        std::cerr << "Error: Could not open output file: " << options.OutputFile << "\n";
        return 1;
      }
      trsc::SymbolTablePrinter STPrinter(outfile, ST);
      STPrinter.print();
    } else {
      trsc::SymbolTablePrinter STPrinter(std::cout, ST);
      STPrinter.print();
    }
    if(options.Verbose) {
        std::cerr << "Exiting after Semantic Analysis (dump-symbol requested)." << "\n";
      }
      return 0;
  }
  if(options.DumpSymbolTable) {
    if (!options.OutputFile.empty()) {
      std::ofstream outfile(options.OutputFile);
      if (!outfile) {
        std::cerr << "Error: Could not open output file: " << options.OutputFile << "\n";
        return 1;
      }
      trsc::SymbolTablePrinter STPrinter(outfile, ST);
      STPrinter.printTree();
    } else {
      trsc::SymbolTablePrinter STPrinter(std::cout, ST);
      STPrinter.printTree();
    }
    if(options.Verbose) {
      std::cerr << "Exiting after Semantic Analysis (dump-symboltable requested)." << "\n";
    }
    return 0;
  }
  if(options.DumpTypedAST) {
    if (!options.OutputFile.empty()) {
      std::ofstream outfile(options.OutputFile);
      if (!outfile) {
        std::cerr << "Error: Could not open output file: " << options.OutputFile << "\n";
        return 1;
      }
      trsc::TypedASTPrinter Printer(outfile);
      Printer.visit(AST.get());
    } else {
      trsc::TypedASTPrinter Printer(std::cout);
      Printer.visit(AST.get());
    }
    if(options.Verbose) {
      std::cerr << "Exiting after Semantic Analysis (dump-typedast requested)." << "\n";
    }
    return 0;
  }

  mlir::MLIRContext MLIRCtx;

  trsc::MLIRGen MLIRGen(MLIRCtx, Ctx, ST);
  mlir::OwningOpRef<mlir::ModuleOp> Module = MLIRGen.genModule(*AST);

  if (!Module) {
    std::cerr << "Error: MLIR generation failed.\n";
    return 1;
  }

  {
    if(options.Verbose) {
      std::cerr << "Running optimization passes.\n";
    }
    mlir::PassManager PM(&MLIRCtx);
    switch (options.Optim) {
      case trsc::OptimizationStage::RawMLIR:
        break;
      case trsc::OptimizationStage::CleanedMLIR:
        mlir::trscd::buildCleanupPipeline(PM);
        break;
      case trsc::OptimizationStage::LoopOptimized:
        mlir::trscd::buildLoopOptPipeline(PM);
        break;
      case trsc::OptimizationStage::StandardLowering:
        break;
      case trsc::OptimizationStage::OptimizedMLIR:
        mlir::trscd::buildCleanupPipeline(PM);
        mlir::trscd::buildLoopOptPipeline(PM);
        break;
      default:
        std::cerr << "Unknown optimization pass.\n";
        break;
    }

    // MatMul recognition/lowering must see trscd ops, so it runs before
    // the LLVM lowering pipeline.
    if (options.Optim != trsc::OptimizationStage::RawMLIR &&
        options.MatMulOptLevel > 0) {
      mlir::trscd::buildMatMulOptPipeline(PM, options.MatMulOptLevel);
    }

    // Translation to LLVM IR requires the module in the LLVM dialect, so
    // every path except -emit-mlir lowers; -emit-mlir only lowers when
    // -optim=stdlowering asks for it.
    if (options.Optim == trsc::OptimizationStage::StandardLowering ||
        !options.EmitMLIR) {
      mlir::trscd::buildLoweringPipeline(PM);
    }

    if(mlir::failed(PM.run(*Module))) {
      std::cerr << "Error: Optimization pipeline failed.\n";
      return 1;
    }
  }

  if(options.EmitMLIR) {
    if(!options.OutputFile.empty()) {
      std::error_code ec;
      llvm::raw_fd_ostream outfile(options.OutputFile, ec, llvm::sys::fs::OF_None);
      if(ec) {
        llvm::errs() << "Error: Could not open output file: " << ec.message() << "\n";
        return 1;
      }
      Module->print(outfile);
    } else {
      Module->print(llvm::outs());
    }
    if(options.Verbose) {
      std::cerr << "Exiting after Emitting MLIR (emit-mlir requested)." << "\n";
    }
    return 0;
  }

  mlir::DialectRegistry TranslationRegistry;
  mlir::registerAllToLLVMIRTranslations(TranslationRegistry);
  Module->getContext()->appendDialectRegistry(TranslationRegistry);

  llvm::LLVMContext LLVMCtx;
  auto LLVMModule = mlir::translateModuleToLLVMIR(*Module, LLVMCtx);
  if (!LLVMModule) {
    std::cerr << "Error: Failed to translate MLIR to LLVM IR.\n";
    return 1;
  }

  if (options.EmitLLVM) {
    if (!options.OutputFile.empty()) {
      std::error_code EC;
      llvm::raw_fd_ostream OutFile(options.OutputFile, EC,
                                   llvm::sys::fs::OF_None);
      if (EC) {
        llvm::errs() << "Error: Could not open output file: " << EC.message()
                     << "\n";
        return 1;
      }
      LLVMModule->print(OutFile, nullptr);
    } else {
      LLVMModule->print(llvm::outs(), nullptr);
    }
    if (options.Verbose) {
      std::cerr << "Exiting after Emitting LLVM IR (emit-llvm requested)."
                << "\n";
    }
    return 0;
  }

  // Native code generation: -emit-obj stops at the object file, the default
  // links it into an executable.
  llvm::InitializeNativeTarget();
  llvm::InitializeNativeTargetAsmPrinter();

  llvm::Triple TheTriple(llvm::sys::getDefaultTargetTriple());
  std::string LookupError;
  const llvm::Target *TheTarget =
      llvm::TargetRegistry::lookupTarget(TheTriple, LookupError);
  if (!TheTarget) {
    std::cerr << "Error: Could not find native target: " << LookupError << "\n";
    return 1;
  }

  llvm::TargetOptions TargetOpts;
  std::unique_ptr<llvm::TargetMachine> TM(
      TheTarget->createTargetMachine(TheTriple, llvm::sys::getHostCPUName(),
                                     "", TargetOpts, llvm::Reloc::PIC_));
  LLVMModule->setTargetTriple(TheTriple);
  LLVMModule->setDataLayout(TM->createDataLayout());

  llvm::SmallString<128> ObjPath;
  if (options.EmitObj) {
    if (!options.OutputFile.empty()) {
      ObjPath = options.OutputFile;
    } else {
      ObjPath = llvm::sys::path::stem(options.InputFile);
      ObjPath += ".o";
    }
  } else {
    if (auto EC = llvm::sys::fs::createTemporaryFile("trsc", "o", ObjPath)) {
      std::cerr << "Error: Could not create temporary object file: "
                << EC.message() << "\n";
      return 1;
    }
  }

  {
    std::error_code EC;
    llvm::raw_fd_ostream ObjFile(ObjPath, EC, llvm::sys::fs::OF_None);
    if (EC) {
      llvm::errs() << "Error: Could not open output file: " << EC.message()
                   << "\n";
      return 1;
    }
    llvm::legacy::PassManager CodegenPasses;
    if (TM->addPassesToEmitFile(CodegenPasses, ObjFile, nullptr,
                                llvm::CodeGenFileType::ObjectFile)) {
      std::cerr << "Error: Native target cannot emit object files.\n";
      return 1;
    }
    CodegenPasses.run(*LLVMModule);
  }

  if (options.EmitObj) {
    if (options.Verbose) {
      std::cerr << "Exiting after Emitting object file (emit-obj requested)."
                << "\n";
    }
    return 0;
  }

  if (options.Verbose) {
    std::cerr << "Linking executable..." << "\n";
  }

  std::string Linker;
  for (const char *Candidate : {"clang", "gcc", "cc"}) {
    if (auto Path = llvm::sys::findProgramByName(Candidate)) {
      Linker = *Path;
      break;
    }
  }
  if (Linker.empty()) {
    std::cerr << "Error: No C compiler driver (clang/gcc/cc) found to link.\n";
    llvm::sys::fs::remove(ObjPath);
    return 1;
  }

  // GPU-lowered code references mgpu* symbols; the static wrapper archive
  // provides them so the binary only needs the system CUDA driver. The
  // archive is only pulled in when referenced, and --as-needed drops the
  // libcuda dependency for CPU-only binaries.
  std::string Output =
      options.OutputFile.empty() ? "a.out" : options.OutputFile;
  std::vector<llvm::StringRef> LinkArgs = {
      Linker, ObjPath, "-o", Output, TRSC_CUDA_RUNTIME_LIB,
      "-Wl,--as-needed", "-lcuda", "-lstdc++", "-lm"};

  std::string LinkError;
  int LinkResult = llvm::sys::ExecuteAndWait(Linker, LinkArgs, std::nullopt,
                                             {}, 0, 0, &LinkError);
  llvm::sys::fs::remove(ObjPath);
  if (LinkResult != 0) {
    std::cerr << "Error: Linking failed";
    if (!LinkError.empty()) {
      std::cerr << ": " << LinkError;
    }
    std::cerr << "\n";
    return 1;
  }

  if (options.Verbose) {
    std::cerr << "Executable written to " << Output << "\n";
  }
  return 0;
}
