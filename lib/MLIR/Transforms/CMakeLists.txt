add_mlir_library(TrscTransforms
  TrscToStandard.cpp
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
  TrscDialect
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

target_precompile_headers(TrscTransforms PRIVATE
  <mlir/Pass/Pass.h>
  <mlir/Transforms/Passes.h>
  <mlir/Dialect/Func/IR/FuncOps.h>
  <mlir/Dialect/SCF/IR/SCF.h>
  <mlir/Dialect/Arith/IR/Arith.h>
  <mlir/Interfaces/SideEffectInterfaces.h>
)

