add_mlir_library(TrscMatMulOpts
  AutoTuning.cpp
  GemmEpilogueFusion.cpp
  GemmLowering.cpp
  MatMulPassPipeline.cpp
  MatMulRecognition.cpp

  DEPENDS
  TrscOpsIncGen
  TrscPassesIncGen

  LINK_LIBS PUBLIC
  MLIRIR
  MLIRPass
  MLIRTransforms
  MLIRSCFDialect
  MLIRMemRefDialect
  MLIRVectorDialect
  MLIRGPUDialect
  MLIRNVVMDialect
  TrscDialect
)

target_include_directories(TrscMatMulOpts PUBLIC
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_BINARY_DIR}/lib/MLIR
)
