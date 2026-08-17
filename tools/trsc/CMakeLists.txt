if(LLVM_LINK_LLVM_DYLIB)
  set(trsc_codegen_libs LLVM)
else()
  llvm_map_components_to_libnames(trsc_codegen_libs core native nativecodegen)
endif()

add_executable(trsc main.cpp)

target_compile_definitions(trsc PRIVATE
  TRSC_VERSION="${TRSC_VERSION}"
  TRSC_CUDA_RUNTIME_LIB="${CMAKE_BINARY_DIR}/lib/libTrscCudaRuntime.a"
  TRSC_PRINT_RUNTIME_LIB="${CMAKE_BINARY_DIR}/lib/libTrscPrintRuntime.a"
)

add_dependencies(trsc TrscCudaRuntime TrscPrintRuntime)

target_link_libraries(trsc PRIVATE
  trsc_lib
  TrscMLIRComponents
  MLIRTargetLLVMIRExport
  MLIRExecutionEngine
  MLIRToLLVMIRTranslationRegistration
  ${trsc_codegen_libs}
)

if(TRSC_GLIBC_COMPAT)
  target_link_libraries(trsc PRIVATE TrscGlibcSymbolVersions)
endif()

install(TARGETS trsc RUNTIME DESTINATION bin)
