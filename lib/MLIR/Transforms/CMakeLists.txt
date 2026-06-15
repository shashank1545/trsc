add_mlir_library(TrscTransforms
  Passes.cpp
  PassPipeline.cpp
  LICM.cpp
  LoopFusion.cpp
  LoopTiling.cpp
  LoopUnrolling.cpp
  
  ADDITIONAL_HEADER_DIRS
  ${CMAKE_SOURCE_DIR}/include/trsc/MLIR
  ${CMAKE_BINARY_DIR}/lib/MLIR
  ${MLIR_INCLUDE_DIRS}
  
  DEPENDS
  TrscPassesIncGen
  
  LINK_LIBS PUBLIC
  MLIRIR
  MLIRSupport
  MLIRFuncDialect
  MLIRArithDialect
  # MLIRControlFlowDialect
  MLIRMemRefDialect
  MLIRLinalgDialect
  MLIRSCFDialect
  MLIRPass
  MLIRTransforms
)

target_include_directories(TrscTransforms PUBLIC
  ${CMAKE_BINARY_DIR}/lib/MLIR
)

