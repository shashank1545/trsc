include(TableGen)
include(AddLLVM)
include(AddMLIR)
include_directories(${MLIR_INCLUDE_DIRS})
include_directories(${CMAKE_BINARY_DIR}/lib)
include_directories(${CMAKE_SOURCE_DIR}/include)

set(MLIR_TABLEGEN_INCLUDE_DIRS
  ${CMAKE_CURRENT_SOURCE_DIR}/IR
  ${CMAKE_CURRENT_SOURCE_DIR}/MLIRGen
  ${CMAKE_CURRENT_SOURCE_DIR}/Transforms
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_BINARY_DIR}/lib/MLIR
  ${MLIR_INCLUDE_DIRS}/include
)

# Generate operations from TableGen files
set(LLVM_TARGET_DEFINITIONS ${CMAKE_SOURCE_DIR}/include/trsc/MLIR/TrscOps.td)
mlir_tablegen(TrscOps.h.inc -gen-op-decls)
mlir_tablegen(TrscOps.cpp.inc -gen-op-defs)
mlir_tablegen(TrscDialect.h.inc -gen-dialect-decls -dialect=trsc)
mlir_tablegen(TrscDialect.cpp.inc -gen-dialect-defs -dialect=trsc)
add_public_tablegen_target(TrscOpsIncGen)

# Generate passes from TableGen files
set(LLVM_TARGET_DEFINITIONS ${CMAKE_SOURCE_DIR}/include/trsc/MLIR/TrscPasses.td)
mlir_tablegen(TrscPasses.h.inc -gen-pass-decls -name Passes)
mlir_tablegen(TrscPasses.cpp.inc -gen-pass-decls -name Passes) 
add_public_tablegen_target(TrscPassesIncGen)

add_subdirectory(IR)
add_subdirectory(Transforms)
add_subdirectory(MLIRGen)

add_library(TrscMLIRComponents INTERFACE)

target_link_libraries(TrscMLIRComponents
  INTERFACE
  TrscDialect 
  TrscToStandard 
  TrscMLIRGen 
)
