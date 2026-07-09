llvm_map_components_to_libnames(trsc_codegen_libs core native nativecodegen)

add_executable(trsc main.cpp)

target_link_libraries(trsc
  PRIVATE
  trsc_lib
  TrscMLIRComponents
  MLIRTargetLLVMIRExport
  MLIRExecutionEngine
  MLIRToLLVMIRTranslationRegistration
  ${trsc_codegen_libs}
)
