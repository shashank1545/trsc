include(FetchContent)

find_package(GTest CONFIG QUIET)
if(NOT GTest_FOUND)
    find_package(GTest QUIET)
endif()

if(NOT GTest_FOUND)
    message(STATUS "Global GoogleTest not found. Fetching from source...")
    
    FetchContent_Declare(
      googletest
      URL https://github.com/google/googletest/archive/refs/heads/main.zip
    )
    FetchContent_MakeAvailable(googletest)
else()
    message(STATUS "Using global system GoogleTest.")
endif()

# For Windows compatibility, but harmless on Linux
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

add_executable(trsc_unit_tests
  AST/ASTAllocatorTest.cpp
  CommandLineTest.cpp
  Lex/LexerTest.cpp
  Parse/MacroCallTest.cpp
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

