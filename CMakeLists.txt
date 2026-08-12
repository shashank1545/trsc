cmake_minimum_required(VERSION 3.20)

if(NOT DEFINED CMAKE_C_COMPILER)
  set(CMAKE_C_COMPILER "/usr/bin/clang")
endif()
if(NOT DEFINED CMAKE_CXX_COMPILER)
  set(CMAKE_CXX_COMPILER "/usr/bin/clang++")
endif()

project(trsc C CXX)

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    message(STATUS "Found ccache: ${CCACHE_PROGRAM}")
    set(CMAKE_C_COMPILER_LAUNCHER   "${CCACHE_PROGRAM}")
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
else()
    message(STATUS "ccache not found. Building without cache.")
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Xclang -fno-pch-timestamp)
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-rpath,/usr/lib")

find_program(TRSC_LLD_LINKER NAMES ld.lld lld)
if(TRSC_LLD_LINKER)
  message(STATUS "Using LLD linker: ${TRSC_LLD_LINKER}")
  set(TRSC_LLD_FLAG "-fuse-ld=${TRSC_LLD_LINKER}")
  set(CMAKE_EXE_LINKER_FLAGS
      "${CMAKE_EXE_LINKER_FLAGS} ${TRSC_LLD_FLAG}")
  set(CMAKE_SHARED_LINKER_FLAGS
      "${CMAKE_SHARED_LINKER_FLAGS} ${TRSC_LLD_FLAG}")
  set(CMAKE_MODULE_LINKER_FLAGS
      "${CMAKE_MODULE_LINKER_FLAGS} ${TRSC_LLD_FLAG}")
else()
  message(STATUS "LLD linker not found; using the compiler default linker")
endif()

option(ENABLE_TESTING "Build the testing suite" OFF)

set(TRSC_REQUESTED_MLIR_DIR "${MLIR_DIR}")
set(TRSC_REQUESTED_LLVM_DIR "${LLVM_DIR}")

find_package(MLIR REQUIRED CONFIG)
find_package(LLVM REQUIRED CONFIG)

if(TRSC_REQUESTED_MLIR_DIR AND
   NOT "${MLIR_DIR}" STREQUAL "${TRSC_REQUESTED_MLIR_DIR}")
  message(FATAL_ERROR
    "Requested MLIR_DIR '${TRSC_REQUESTED_MLIR_DIR}', but CMake resolved "
    "'${MLIR_DIR}'. Refusing an unexpected MLIR installation.")
endif()
if(TRSC_REQUESTED_LLVM_DIR AND
   NOT "${LLVM_DIR}" STREQUAL "${TRSC_REQUESTED_LLVM_DIR}")
  message(FATAL_ERROR
    "Requested LLVM_DIR '${TRSC_REQUESTED_LLVM_DIR}', but CMake resolved "
    "'${LLVM_DIR}'. Refusing an unexpected LLVM installation.")
endif()

if(ENABLE_TESTING)
  enable_testing()
  add_subdirectory(test)
endif()

list(APPEND CMAKE_MODULE_PATH "${MLIR_CMAKE_DIR}")
list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")

include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${MLIR_INCLUDE_DIRS})
include_directories(${LLVM_INCLUDE_DIRS})

add_subdirectory(lib)
add_subdirectory(lib/MLIR)
add_subdirectory(lib/Runtime)
add_subdirectory(tools)
