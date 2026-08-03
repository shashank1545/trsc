#include <fstream>
#include <system_error>

#include "mlir/Pass/PassManager.h"
#include "mlir/Support/LLVM.h"
#include "mlir/Target/LLVMIR/Dialect/All.h"
#include "mlir/Target/LLVMIR/Export.h"

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/CodeGen.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/Program.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/TargetParser/Host.h"

#include "trsc/AST/ASTContext.h"
#include "trsc/AST/ASTPrinter.h"
#include "trsc/AST/TypedASTPrinter.h"
#include "trsc/Basic/CommandLine.h"
#include "trsc/Basic/IdentifierTable.h"
#include "trsc/Lex/Lexer.h"
#include "trsc/MLIR/MatMulOpts/MatMulOptPasses.h"
#include "trsc/MLIR/Transforms/PassPipeline.h"
#include "trsc/MLIR/TrscMLIRGen.h"
#include "trsc/Parse/Parser.h"
#include "trsc/Sema/Sema.h"
#include "trsc/Sema/SymbolTablePrinter.h"

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
  trsc::IdentifierTable Idents;
  trsc::Lex::Lexer Lex(SM, Diag, Idents);
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
    std::cerr << "Lexical Analysis complete. " << Tokens.size()
              << " tokens found." << "\n";
  }

  if (options.DumpLexerTokens) {
    if (options.Verbose) {
      std::cerr << "Exiting after Lexical Analysis (dump-tokens requested)."
                << "\n";
    }
    return 0;
  }

  if (options.Verbose) {
    std::cerr << "Starting Parsing..." << "\n";
  }

  // The context owns the AST arena, so it must outlive every consumer of the
  // tree - it is constructed before the Parser and destroyed after MLIRGen.
  trsc::ASTContext Ctx;
  trsc::Parser Parser(Ctx, Diag, Tokens);
  trsc::Program *AST = Parser.parse();

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
          std::cerr << "Error: Could not open output file: "
                    << options.OutputFile << "\n";
          return 1;
        }
        trsc::ASTPrinter printer(outfile);
        printer.visit(AST);
      } else {
        trsc::ASTPrinter printer(std::cout);
        printer.visit(AST);
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

  trsc::SymbolTable ST(Idents);
  trsc::SemanticAnalyzer Sema(Diag, ST, Ctx);
  Sema.analyze(AST);

  if (Diag.getNumErrors() > 0) {
    std::cerr << "Semantic analysis failed with " << Diag.getNumErrors()
              << " errors."
              << "\n";
    return 1;
  }
  if (options.Verbose) {
    std::cerr << "Semantic Analysis complete." << "\n";
  }
  if (options.DumpSymbol) {
    if (!options.OutputFile.empty()) {
      std::ofstream outfile(options.OutputFile);
      if (!outfile) {
        std::cerr << "Error: Could not open output file: " << options.OutputFile
                  << "\n";
        return 1;
      }
      trsc::SymbolTablePrinter STPrinter(outfile, ST);
      STPrinter.print();
    } else {
      trsc::SymbolTablePrinter STPrinter(std::cout, ST);
      STPrinter.print();
    }
    if (options.Verbose) {
      std::cerr << "Exiting after Semantic Analysis (dump-symbol requested)."
                << "\n";
    }
    return 0;
  }
  if (options.DumpSymbolTable) {
    if (!options.OutputFile.empty()) {
      std::ofstream outfile(options.OutputFile);
      if (!outfile) {
        std::cerr << "Error: Could not open output file: " << options.OutputFile
                  << "\n";
        return 1;
      }
      trsc::SymbolTablePrinter STPrinter(outfile, ST);
      STPrinter.printTree();
    } else {
      trsc::SymbolTablePrinter STPrinter(std::cout, ST);
      STPrinter.printTree();
    }
    if (options.Verbose) {
      std::cerr
          << "Exiting after Semantic Analysis (dump-symboltable requested)."
          << "\n";
    }
    return 0;
  }
  if (options.DumpTypedAST) {
    if (!options.OutputFile.empty()) {
      std::ofstream outfile(options.OutputFile);
      if (!outfile) {
        std::cerr << "Error: Could not open output file: " << options.OutputFile
                  << "\n";
        return 1;
      }
      trsc::TypedASTPrinter Printer(outfile);
      Printer.visit(AST);
    } else {
      trsc::TypedASTPrinter Printer(std::cout);
      Printer.visit(AST);
    }
    if (options.Verbose) {
      std::cerr << "Exiting after Semantic Analysis (dump-typedast requested)."
                << "\n";
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
    if (options.Verbose) {
      std::cerr << "Running optimization passes.\n";
    }
    mlir::PassManager PM(&MLIRCtx);
    // Every major optimization stage is followed by a cleaning round
    // (canonicalize + CSE). For code-emitting paths (-emit-llvm, -emit-obj,
    // linking) the full pipeline always runs before lowering; -emit-mlir
    // uses -optim to select how far the pipeline runs so each stage can be
    // inspected.
    trsc::OptimizationStage Stage =
        options.EmitMLIR ? options.Optim
                         : trsc::OptimizationStage::StandardLowering;

    if (Stage != trsc::OptimizationStage::RawMLIR) {
      mlir::trscd::buildCleanupPipeline(PM);
      mlir::trscd::buildMem2RegPipeline(PM);
      mlir::trscd::buildCleanupPipeline(PM);
    }
    if (Stage != trsc::OptimizationStage::RawMLIR &&
        Stage != trsc::OptimizationStage::CleanedMLIR) {
      mlir::trscd::buildLoopOptPipeline(PM);
      mlir::trscd::buildCleanupPipeline(PM);
    }
    if (Stage == trsc::OptimizationStage::OptimizedMLIR ||
        Stage == trsc::OptimizationStage::StandardLowering) {
      if (options.MatMulOptLevel > 0) {
        mlir::trscd::buildMatMulOptPipeline(PM, options.MatMulOptLevel,
                                            options.Target);
        mlir::trscd::buildCleanupPipeline(PM);
      }
      mlir::trscd::buildLateLoopOptPipeline(PM);
      mlir::trscd::buildCleanupPipeline(PM);
    }
    if (Stage == trsc::OptimizationStage::StandardLowering) {
      mlir::trscd::buildLoweringPipeline(PM, options.Target.CudaArch);
    }

    if (mlir::failed(PM.run(*Module))) {
      std::cerr << "Error: Optimization pipeline failed.\n";
      return 1;
    }
  }

  if (options.EmitMLIR) {
    if (!options.OutputFile.empty()) {
      std::error_code ec;
      llvm::raw_fd_ostream outfile(options.OutputFile, ec,
                                   llvm::sys::fs::OF_None);
      if (ec) {
        llvm::errs() << "Error: Could not open output file: " << ec.message()
                     << "\n";
        return 1;
      }
      Module->print(outfile);
    } else {
      Module->print(llvm::outs());
    }
    if (options.Verbose) {
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
      TheTarget->createTargetMachine(TheTriple, llvm::sys::getHostCPUName(), "",
                                     TargetOpts, llvm::Reloc::PIC_));
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
    if (std::error_code ec = llvm::sys::fs::remove(ObjPath)) {
      std::cerr << "Warning: Failed to remove temporary file '"
                << ObjPath.c_str() << "': " << ec.message() << "\n";
    }
    return 1;
  }

  // GPU-lowered code uses a lazily loaded CUDA driver table. Auto-dispatch
  // binaries therefore start and run their CPU path without libcuda.
  std::string Output =
      options.OutputFile.empty() ? "a.out" : options.OutputFile;
  std::vector<llvm::StringRef> LinkArgs = {Linker,
                                           ObjPath,
                                           "-o",
                                           Output,
                                           TRSC_CUDA_RUNTIME_LIB,
                                           "-Wl,--as-needed",
                                           "-ldl",
                                           "-lstdc++",
                                           "-lm"};

  std::string LinkError;
  int LinkResult = llvm::sys::ExecuteAndWait(Linker, LinkArgs, std::nullopt, {},
                                             0, 0, &LinkError);
  if (std::error_code ec = llvm::sys::fs::remove(ObjPath)) {
    std::cerr << "Warning: Failed to remove temporary file '" << ObjPath.c_str()
              << "': " << ec.message() << "\n";
  }
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
