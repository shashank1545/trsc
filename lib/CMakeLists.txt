set(TRSC_LIB
  ${CMAKE_CURRENT_SOURCE_DIR}/Lex/Token.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/Lex/Lexer.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/Basic/Diagnostics.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/Basic/SourceManager.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/Basic/CommandLine.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/AST/AST.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/AST/ASTPrinter.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/AST/TypedASTPrinter.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/AST/ASTContext.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/AST/QualType.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/Parse/Parser.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/Sema/Scope.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/Sema/SymbolTable.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/Sema/DeclarationCollector.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/Sema/NameResolver.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/Sema/TypeChecker.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/Sema/BorrowChecker.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/Sema/Sema.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/Sema/SymbolTablePrinter.cpp
)


add_library(trsc_lib STATIC ${TRSC_LIB})

target_include_directories(trsc_lib PUBLIC ${CMAKE_SOURCE_DIR}/include)
