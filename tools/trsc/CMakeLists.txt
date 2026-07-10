llvm_map_components_to_libnames(trsc_codegen_libs core native nativecodegen)

add_executable(trsc main.cpp)

target_compile_definitions(trsc PRIVATE
  TRSC_CUDA_RUNTIME_LIB="${CMAKE_BINARY_DIR}/lib/libTrscCudaRuntime.a"
)

add_dependencies(trsc TrscCudaRuntime)

target_link_libraries(trsc
  PRIVATE
  trsc_lib
  TrscMLIRComponents
  MLIRTargetLLVMIRExport
  MLIRExecutionEngine
  MLIRToLLVMIRTranslationRegistration
  ${trsc_codegen_libs}
)
