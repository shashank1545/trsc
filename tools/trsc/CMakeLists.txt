add_executable(trsc main.cpp)

target_link_libraries(trsc
  PRIVATE
  trsc_lib
  TrscMLIRComponents 
)
