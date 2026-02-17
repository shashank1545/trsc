#include <vector>
#include <iostream>
#include <fstream>

#include "trsc/AST/AST.h"
#include "trsc/Lex/Lexer.h"
#include "trsc/Parse/Parser.h"
#include "trsc/AST/ASTPrinter.h"
#include "trsc/Basic/CommandLine.h"
#include "trsc/Basic/Diagnostics.h"
#include "trsc/Basic/SourceManager.h"
#include "trsc/Sema/Sema.h" 
#include "trsc/Sema/SymbolTable.h"
#include "trsc/Sema/SymbolTablePrinter.h"
#include "trsc/AST/TypedASTPrinter.h"

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

  trsc::SemanticAnalyzer Sema(Diag);
  Sema.analyze(AST.get());

  if (Diag.getNumErrors() > 0) {
    std::cerr << "Semantic analysis failed with " << Diag.getNumErrors() << " errors."
      << "\n";
    return 1;
  }
  if (options.Verbose) {
    std::cerr << "Semantic Analysis complete." << "\n";
  }
  trsc::SymbolTable& ST = Sema.getSymbolTable();
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
}
