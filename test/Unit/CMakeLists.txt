include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/heads/main.zip
)
# For Windows compatibility, but harmless on Linux
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Define the unit test binary
add_executable(trsc_unit_tests
  Lex/LexerTest.cpp
  Sema/SymbolTableTest.cpp
)

# Link against GTest and your compiler's library
# We assume trsc_lib is defined in lib/CMakeLists.txt
target_link_libraries(trsc_unit_tests 
  PRIVATE
  GTest::gtest_main 
  trsc_lib
)

# Enable CTest to discover the GTests automatically
include(GoogleTest)
gtest_discover_tests(trsc_unit_tests
  PROPERTIES LABELS "Unit")

# # Integration with Lit for regression tests
# # This can be expanded to generate lit.site.cfg.py if needed.
# find_program(LIT_EXECUTABLE NAMES lit llvm-lit)
# if (NOT LIT_EXECUTABLE)
# message(WARNING "Could not find lit or llvm-lit. Regression tests will fail.")
# endif()
#
# add_custom_target(check-trsc
#   COMMAND ${LIT_EXECUTABLE} -v ${CMAKE_CURRENT_SOURCE_DIR}
#   DEPENDS trsc trsc-opt
#   COMMENT "Running all trsc tests..."
# )
#
# add_custom_target(check-trsc-regression
#   COMMAND ${LIT_EXECUTABLE} -v ${CMAKE_CURRENT_SOURCE_DIR}/Regression
#   DEPENDS trsc trsc-opt
#   COMMENT "Running trsc regression tests..."
# )

