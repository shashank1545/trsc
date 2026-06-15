add_mlir_dialect_library(TrscDialect
  TrscDialect.cpp
  TrscOps.cpp
  
  ADDITIONAL_HEADER_DIRS
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_BINARY_DIR}/lib/MLIR
  ${MLIR_INCLUDE_DIRS}
  
  DEPENDS
  TrscOpsIncGen
  
  LINK_LIBS PUBLIC
  MLIRIR
  MLIRInferTypeOpInterface
  MLIRSupport
)

target_include_directories(TrscDialect PUBLIC
  ${CMAKE_SOURCE_DIR}/include/trsc/MLIR
  ${CMAKE_BINARY_DIR}/lib/MLIR
  ${MLIR_INCLUDE_DIRS}
)
