cmake_minimum_required(VERSION 3.20)

if(NOT DEFINED CMAKE_C_COMPILER)
  set(CMAKE_C_COMPILER "/usr/bin/clang")
endif()
if(NOT DEFINED CMAKE_CXX_COMPILER)
  set(CMAKE_CXX_COMPILER "/usr/bin/clang++")
endif()

project(trsc C CXX)

set(TRSC_VERSION "0.0.0-dev" CACHE STRING "Version reported by trsc")

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
set(TRSC_RPATH "" CACHE STRING "Runtime library search path baked into 
binaries")
if(TRSC_RPATH)
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-rpath,
  ${TRSC_RPATH}")
endif()

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
option(TRSC_LINK_LLVM_DYLIB
       "Link trsc against LLVM's shared library"
       OFF)
option(TRSC_BUILD_OPT "Build the trsc-opt development tool" ON)
set(TRSC_LLVM_STATIC_ARCHIVE "" CACHE FILEPATH
    "Combined static LLVM archive for distro LLVM packages")
set(TRSC_MLIR_STATIC_ARCHIVE "" CACHE FILEPATH
    "Combined static MLIR archive for distro MLIR packages")

# Building on a host with a very new glibc stamps the newest available symbol
# version onto every libc/libm reference, which makes the binary unloadable on
# older distros.  lib/Support/GlibcSymbolVersions.c re-points those references
# at the old ABI; see that file for the exact list and the resulting floor.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND
   CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
  set(TRSC_GLIBC_COMPAT_DEFAULT ON)
else()
  set(TRSC_GLIBC_COMPAT_DEFAULT OFF)
endif()
option(TRSC_GLIBC_COMPAT
       "Pin glibc symbol versions so the binary runs on older distros"
       ${TRSC_GLIBC_COMPAT_DEFAULT})

set(TRSC_REQUESTED_MLIR_DIR "${MLIR_DIR}")
set(TRSC_REQUESTED_LLVM_DIR "${LLVM_DIR}")

find_package(MLIR REQUIRED CONFIG)
find_package(LLVM REQUIRED CONFIG)

set(LLVM_LINK_LLVM_DYLIB ${TRSC_LINK_LLVM_DYLIB})
set(MLIR_LINK_MLIR_DYLIB OFF)

if(TRSC_LLVM_STATIC_ARCHIVE OR TRSC_MLIR_STATIC_ARCHIVE)
  if(TRSC_LINK_LLVM_DYLIB)
    message(FATAL_ERROR
      "TRSC_*_STATIC_ARCHIVE requires TRSC_LINK_LLVM_DYLIB=OFF")
  endif()
  foreach(TRSC_STATIC_TARGET_ARCHIVE IN ITEMS
      "LLVM|${TRSC_LLVM_STATIC_ARCHIVE}"
      "MLIR|${TRSC_MLIR_STATIC_ARCHIVE}")
    string(REPLACE "|" ";" TRSC_STATIC_TARGET_ARCHIVE
      "${TRSC_STATIC_TARGET_ARCHIVE}")
    list(GET TRSC_STATIC_TARGET_ARCHIVE 0 TRSC_STATIC_TARGET)
    list(GET TRSC_STATIC_TARGET_ARCHIVE 1 TRSC_STATIC_ARCHIVE)
    if(TRSC_STATIC_ARCHIVE)
      if(NOT EXISTS "${TRSC_STATIC_ARCHIVE}")
        message(FATAL_ERROR
          "Static archive does not exist: ${TRSC_STATIC_ARCHIVE}")
      endif()
      if(NOT TARGET ${TRSC_STATIC_TARGET})
        message(FATAL_ERROR
          "LLVM/MLIR package did not define target ${TRSC_STATIC_TARGET}")
      endif()
      set_target_properties(${TRSC_STATIC_TARGET} PROPERTIES
        IMPORTED_LOCATION "${TRSC_STATIC_ARCHIVE}"
        IMPORTED_LOCATION_DEBUG "${TRSC_STATIC_ARCHIVE}"
        IMPORTED_LOCATION_MINSIZEREL "${TRSC_STATIC_ARCHIVE}"
        IMPORTED_LOCATION_RELEASE "${TRSC_STATIC_ARCHIVE}"
        IMPORTED_LOCATION_RELWITHDEBINFO "${TRSC_STATIC_ARCHIVE}"
        IMPORTED_NO_SONAME TRUE
        IMPORTED_SONAME "")
    endif()
  endforeach()

  if(TARGET LLVMSupport)
    get_target_property(TRSC_LLVM_SUPPORT_DEPS
      LLVMSupport INTERFACE_LINK_LIBRARIES)
    if(TRSC_LLVM_SUPPORT_DEPS)
      list(FILTER TRSC_LLVM_SUPPORT_DEPS EXCLUDE REGEX "z3")
      set_target_properties(LLVMSupport PROPERTIES
        INTERFACE_LINK_LIBRARIES "${TRSC_LLVM_SUPPORT_DEPS}")
    endif()
  endif()
endif()

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
add_subdirectory(lib/Support)
add_subdirectory(tools)
