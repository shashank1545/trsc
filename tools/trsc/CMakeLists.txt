add_executable(trsc main.cpp)

target_link_libraries(trsc
  PRIVATE
  trsc_lib
  trsc_mlir_components
)
