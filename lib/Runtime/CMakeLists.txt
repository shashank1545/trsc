add_library(TrscPrintRuntime STATIC PrintRuntime.cpp)

set_target_properties(TrscPrintRuntime PROPERTIES
  ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)

# Static CUDA runtime wrappers (vendored from mlir/lib/ExecutionEngine) so
# trsc-produced binaries do not depend on the LLVM build tree's
# libmlir_cuda_runtime.so. The CUDA driver is loaded lazily so emitted
# auto-dispatch executables also run on CPU-only systems without libcuda.
find_package(CUDAToolkit REQUIRED)

add_library(TrscCudaRuntime STATIC CudaRuntimeWrappers.cpp)

target_link_libraries(TrscCudaRuntime PUBLIC ${CMAKE_DL_LIBS})

target_include_directories(TrscCudaRuntime PRIVATE
  ${MLIR_INCLUDE_DIRS}
  ${CUDAToolkit_INCLUDE_DIRS}
)

set_target_properties(TrscCudaRuntime PROPERTIES
  ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)
