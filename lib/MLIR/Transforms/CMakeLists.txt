add_mlir_library(TrscToStandard
  TrscToStandard.cpp
  Passes.cpp
  
  ADDITIONAL_HEADER_DIRS
  ${CMAKE_SOURCE_DIR}/include/trsc/MLIR
  ${CMAKE_BINARY_DIR}/lib/MLIR
  ${MLIR_INCLUDE_DIRS}
  
  DEPENDS
  TrscPassesIncGen
  
  LINK_LIBS PUBLIC
  TrscDialect
  MLIRFuncDialect
  MLIRArithDialect
  MLIRControlFlowDialect
  MLIRPass
  MLIRTransforms
)

target_include_directories(TrscToStandard PUBLIC
  ${CMAKE_BINARY_DIR}/lib/MLIR
)

