add_executable(trsc main.cpp)

target_link_libraries(trsc trsc_lib)

target_include_directories(trsc PUBLIC ${CMAKE_SOURCE_DIR}/include)


