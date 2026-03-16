cmake_minimum_required(VERSION 3.20)

set(CMAKE_CXX_COMPILER "clang++")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(trsc C CXX)

enable_testing()

include_directories(${CMAKE_SOURCE_DIR}/include)

find_package(MLIR REQUIRED CONFIG)
find_package(LLVM REQUIRED CONFIG)
include_directories(${MLIR_INCLUDE_DIRS})
include_directories(${LLVM_INCLUDE_DIRS})

list(APPEND CMAKE_MODULE_PATH "${MLIR_CMAKE_DIR}")
list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")

add_subdirectory(lib)
add_subdirectory(lib/MLIR)
add_subdirectory(tools)
add_subdirectory(test)

# add_executable(trsc tools/trsc/main.cpp)
# target_link_libraries(trsc
#   PRIVATE
#   trsc_lib
#   trsc_mlir_components
# )
#
# add_executable(trsc-opt tools/trsc-opt/main.cpp)
# target_link_libraries(trsc-opt
#   PRIVATE
#   ${MLIR_DIALECT_LIBS}
#   ${MLIR_CONVERSION_LIBS}
# )
#
# get_property(MLIR_DIALECT_LIBS GLOBAL PROPERTY MLIR_DIALECT_LIBS)
# get_property(MLIR_CONVERSION_LIBS GLOBAL PROPERTY MLIR_CONVERSION_LIBS)
#
