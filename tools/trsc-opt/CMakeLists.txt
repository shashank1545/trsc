set(LIBS
  TrscDialect
  TrscTransforms
  MLIRIR
  MLIRPass
  MLIRTransforms
  MLIRAnalysis
  MLIRParser
  MLIRSupport
  MLIROptLib
  MLIRFuncDialect
  MLIRArithDialect
  MLIRSCFDialect
  MLIRMemRefDialect
  MLIRLinalgDialect
  MLIRControlFlowDialect
  MLIRSCFToControlFlow
)

add_llvm_executable(trsc-opt
    main.cpp
)

# Update compile flags for LLVM
llvm_update_compile_flags(trsc-opt)

# Link libraries
target_link_libraries(trsc-opt PRIVATE ${LIBS})

# Add include directories
target_include_directories(trsc-opt PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_BINARY_DIR}/lib/MLIR
)

# Check all link libraries 
mlir_check_all_link_libraries(trsc-opt)
