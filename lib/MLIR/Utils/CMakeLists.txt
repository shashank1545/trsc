add_mlir_library(TrscUtils
  Utils.cpp
  
  ADDITIONAL_HEADER_DIRS
  ${CMAKE_SOURCE_DIR}/include/trsc/MLIR
  ${CMAKE_BINARY_DIR}/lib/MLIR
  ${MLIR_INCLUDE_DIRS}
  
  LINK_LIBS PUBLIC
  MLIRIR
  MLIRSupport
  MLIRMemRefDialect
  MLIRSCFDialect
)

target_include_directories(TrscUtils PUBLIC
  ${CMAKE_BINARY_DIR}/lib/MLIR
)
