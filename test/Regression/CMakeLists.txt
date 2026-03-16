# Find the lit executable directly — do NOT use python3 -m lit.
# lit installed via pip lands in the Python scripts directory (e.g. 
# ~/.local/bin/lit or /opt/miniconda3/bin/lit). 
# find_program searches PATH for it.
find_program(LIT_EXECUTABLE
  NAMES lit lit.py
  HINTS
    ${Python3_ROOT_DIR}/bin          # conda env bin/
    "$ENV{HOME}/.local/bin"          # pip install --user
    /usr/local/bin
  REQUIRED
)
message(STATUS "Found lit: ${LIT_EXECUTABLE}")

# --- Fixture: build trsc before any regression test runs ---
# CTest fixtures let you declare "run this test first, and if it fails,
# skip all tests that require it."
add_test(
  NAME regression_build
  COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target trsc
)
set_tests_properties(regression_build PROPERTIES
  LABELS   "Regression"
  FIXTURES_SETUP regression_fixture
)

# Pass each subdirectory as a separate argument so lit discovers all of them.
add_test(
  NAME regression
  COMMAND ${LIT_EXECUTABLE} -v
    ${CMAKE_CURRENT_SOURCE_DIR}/Ops
    ${CMAKE_CURRENT_SOURCE_DIR}/Sema
    ${CMAKE_CURRENT_SOURCE_DIR}/Lex
    ${CMAKE_CURRENT_SOURCE_DIR}/Parse
)
set_tests_properties(regression PROPERTIES
  LABELS            "Regression"
  FIXTURES_REQUIRED regression_fixture
)
