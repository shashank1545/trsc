# Get all dialect and conversion libraries
get_property(dialect_libs GLOBAL PROPERTY MLIR_DIALECT_LIBS)
get_property(conversion_libs GLOBAL PROPERTY MLIR_CONVERSION_LIBS)

# Define all libraries to link against
set(LIBS
    ${dialect_libs}
    ${conversion_libs}
    # Your TRSC libraries
    TrscDialect
    TrscToStandard
    TrscMLIRGen
    # MLIR Core
    MLIRAnalysis
    MLIRIR
    MLIRParser
    MLIRPass
    MLIRSupport
    MLIRTransforms
    MLIROptLib
)

# Use add_llvm_executable (not add_executable!)
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

# Check all link libraries (helps catch linking issues)
mlir_check_all_link_libraries(trsc-opt)
