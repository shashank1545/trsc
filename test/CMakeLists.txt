cmake_policy(SET CMP0135 NEW)
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
  unit/Lex/LexerTest.cpp
  unit/Sema/SymbolTableTest.cpp
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
gtest_discover_tests(trsc_unit_tests)

# Integration with Lit for regression tests
# This can be expanded to generate lit.site.cfg.py if needed.
add_custom_target(check-trsc
  COMMAND ${PYTHON_EXECUTABLE} -m lit -v ${CMAKE_CURRENT_SOURCE_DIR}
  DEPENDS trsc trsc-opt
  COMMENT "Running trsc regression tests..."
)
