add_mlir_library(TrscMLIRGen
  MLIRGen.cpp
  
  ADDITIONAL_HEADER_DIRS
  ${CMAKE_SOURCE_DIR}/include/trsc/MLIR
  ${CMAKE_BINARY_DIR}/lib/MLIR
  ${MLIR_INCLUDE_DIRS}
  
  DEPENDS
  TrscOpsIncGen
  
  LINK_LIBS PUBLIC
  TrscDialect
  MLIRIR
  MLIRSupport
  MLIRFuncDialect
)

target_include_directories(TrscMLIRGen PUBLIC
  ${CMAKE_BINARY_DIR}/lib/MLIR
)
