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
  TrscUtils
  MLIRIR
  MLIRSupport
  MLIRFuncDialect
  MLIRArithDialect
  # MLIRControlFlowDialect
  MLIRMemRefDialect
  MLIRLinalgDialect
  MLIRLinalgTransforms
  MLIRSCFDialect
  MLIRSCFUtils
  MLIRPass
  MLIRTransforms
  MLIRGPUTransforms
  MLIRGPUPipelines
  MLIRGPUToNVVMTransforms
  MLIRGPUToGPURuntimeTransforms
  MLIRLLVMDialect
  MLIRNVVMDialect
  MLIRTargetLLVMIRExport
  MLIRNVVMToLLVMIRTranslation
)

target_include_directories(TrscTransforms PUBLIC
  ${CMAKE_BINARY_DIR}/lib/MLIR
)

foreach(tgt TrscTransforms obj.TrscTransforms)
  if(TARGET ${tgt})
    target_precompile_headers(${tgt} PRIVATE
      <mlir/Pass/Pass.h>
      <mlir/Dialect/Func/IR/FuncOps.h>
      <mlir/Dialect/SCF/IR/SCF.h>
      <mlir/Dialect/Arith/IR/Arith.h>
      <mlir/Interfaces/SideEffectInterfaces.h>
    )
  endif()
endforeach()
