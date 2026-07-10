# Static CUDA runtime wrappers (vendored from mlir/lib/ExecutionEngine) so
# trsc-produced binaries do not depend on the LLVM build tree's
# libmlir_cuda_runtime.so. Linked statically into every emitted executable;
# the only remaining runtime dependency is the system CUDA driver (libcuda).
find_package(CUDAToolkit REQUIRED)

add_library(TrscCudaRuntime STATIC CudaRuntimeWrappers.cpp)

target_include_directories(TrscCudaRuntime PRIVATE
  ${MLIR_INCLUDE_DIRS}
  ${CUDAToolkit_INCLUDE_DIRS}
)

set_target_properties(TrscCudaRuntime PROPERTIES
  ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)
