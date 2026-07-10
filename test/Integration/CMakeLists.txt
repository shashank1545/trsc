# --- End-to-end integration tests ---
# Each test drives the full compiler: .rs source -> MLIR pipeline -> LLVM IR
# -> object file -> linked binary -> run and verify the numeric result.
#
# The f32 return from main makes the process exit code useless, so the
# compiled main is renamed to trsc_main (objcopy) and linked against
# harness.c, which prints the result for exact comparison (64.0 for the
# 32x32 all-ones x all-twos matmul).

set(TRSC_INTEGRATION_RUNTIME ${CMAKE_BINARY_DIR}/lib/libTrscCudaRuntime.a)

# Levels 0-1 lower to CPU loops; levels 2-6 launch CUDA kernels and need a
# working GPU at test time (labelled GPU so they can be excluded with
# `ctest -LE GPU`).
foreach(MATMUL_LEVEL RANGE 0 6)
  add_test(
    NAME integration_matmul_l${MATMUL_LEVEL}
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/run_matmul_test.sh
      $<TARGET_FILE:trsc>
      ${MATMUL_LEVEL}
      ${CMAKE_CURRENT_SOURCE_DIR}/matmul.rs
      ${CMAKE_CURRENT_SOURCE_DIR}/harness.c
      ${TRSC_INTEGRATION_RUNTIME}
      ${CMAKE_OBJCOPY}
      ${CMAKE_C_COMPILER}
      64.0
  )
  if(MATMUL_LEVEL GREATER 1)
    set_tests_properties(integration_matmul_l${MATMUL_LEVEL} PROPERTIES
      LABELS            "Integration;GPU"
      FIXTURES_REQUIRED regression_fixture
    )
  else()
    set_tests_properties(integration_matmul_l${MATMUL_LEVEL} PROPERTIES
      LABELS            "Integration"
      FIXTURES_REQUIRED regression_fixture
    )
  endif()
endforeach()

# The driver's own link step: no -emit flag makes trsc link the executable
# itself. Verifies the end-to-end binary is produced and runs to completion
# (exit status below 128 means it was not killed by a signal; the value
# itself is the truncated f32 return and is not meaningful).
add_test(
  NAME integration_driver_link
  COMMAND bash -c "\
    work=$(mktemp -d) && trap 'rm -rf \"$work\"' EXIT; \
    $<TARGET_FILE:trsc> --matmul-opt-level=6 \
      ${CMAKE_CURRENT_SOURCE_DIR}/matmul.rs -o \"$work/mm\" && \
    test -x \"$work/mm\" || exit 1; \
    \"$work/mm\"; status=$?; \
    test $status -lt 128"
)
set_tests_properties(integration_driver_link PROPERTIES
  LABELS            "Integration;GPU"
  FIXTURES_REQUIRED regression_fixture
)
