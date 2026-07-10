// RUN: %trsc-opt --trsc-loop-unroll=unroll-factor=4 %s | %FileCheck %s
// RUN: %trsc-opt --trsc-loop-unroll=unroll-factor=1 %s | %FileCheck %s --check-prefix=UF1

// ============================================================================
// TEST 1: Trip count divisible by factor — body replicated 4x, no epilogue
// ============================================================================

// CHECK-LABEL: func.func @unroll_divisible
// CHECK:         %[[C4:.*]] = arith.constant 4 : index
// CHECK:         scf.for %[[IV:.*]] = %{{.*}} to %{{.*}} step %[[C4]]
// CHECK-COUNT-4:   memref.store
// CHECK:         }
// CHECK-NOT:     scf.for
// CHECK:         return

// UF1-LABEL: func.func @unroll_divisible
// UF1-NOT:     arith.muli
func.func @unroll_divisible(%A: memref<16xf32>) {
  %c0 = arith.constant 0 : index
  %c16 = arith.constant 16 : index
  %c1 = arith.constant 1 : index
  scf.for %i = %c0 to %c16 step %c1 {
    %v = memref.load %A[%i] : memref<16xf32>
    memref.store %v, %A[%i] : memref<16xf32>
  }
  return
}

// ============================================================================
// TEST 2: Trip count NOT divisible by factor — unrolled main loop + epilogue
// ============================================================================
// 10 iterations, factor 4: main loop covers [0, 8) by 4, epilogue [8, 10) by 1.

// CHECK-LABEL: func.func @unroll_epilogue
// CHECK-DAG:     %[[C8:.*]] = arith.constant 8 : index
// CHECK-DAG:     %[[C4:.*]] = arith.constant 4 : index
// CHECK:         scf.for %{{.*}} = %{{.*}} to %[[C8]] step %[[C4]]
// CHECK-COUNT-4:   memref.store
// CHECK:         }
// CHECK:         scf.for %{{.*}} = %[[C8]] to %{{.*}} step %{{.*}}
// CHECK:           memref.load
// CHECK:           memref.store
// CHECK-NOT:       memref.store
// CHECK:         }

// UF1-LABEL: func.func @unroll_epilogue
// UF1-NOT:     arith.muli
func.func @unroll_epilogue(%A: memref<10xf32>) {
  %c0 = arith.constant 0 : index
  %c10 = arith.constant 10 : index
  %c1 = arith.constant 1 : index
  scf.for %i = %c0 to %c10 step %c1 {
    %v = memref.load %A[%i] : memref<10xf32>
    memref.store %v, %A[%i] : memref<10xf32>
  }
  return
}

// ============================================================================
// TEST 3: Only the innermost loop of a nest is unrolled
// ============================================================================
// Outer loop keeps its unit step; inner loop steps by the factor.

// CHECK-LABEL: func.func @unroll_innermost_only
// CHECK:         %[[C1:.*]] = arith.constant 1 : index
// CHECK:         scf.for %{{.*}} = %{{.*}} to %{{.*}} step %[[C1]]
// CHECK:           %[[C4:.*]] = arith.constant 4 : index
// CHECK:           scf.for %{{.*}} = %{{.*}} to %{{.*}} step %[[C4]]
// CHECK-COUNT-4:     memref.store
// CHECK:           }
// CHECK:         }
func.func @unroll_innermost_only(%A: memref<8x8xf32>) {
  %c0 = arith.constant 0 : index
  %c8 = arith.constant 8 : index
  %c1 = arith.constant 1 : index
  scf.for %i = %c0 to %c8 step %c1 {
    scf.for %j = %c0 to %c8 step %c1 {
      %v = memref.load %A[%i, %j] : memref<8x8xf32>
      memref.store %v, %A[%i, %j] : memref<8x8xf32>
    }
  }
  return
}

// ============================================================================
// TEST 4: Loop with iter_args — accumulator chain replicated, single yield
// ============================================================================

// CHECK-LABEL: func.func @unroll_iter_args
// CHECK:         %[[C4:.*]] = arith.constant 4 : index
// CHECK:         scf.for %{{.*}} = %{{.*}} to %{{.*}} step %[[C4]] iter_args(%[[ACC:.*]] = %{{.*}})
// CHECK-COUNT-4:   arith.addf
// CHECK:           scf.yield
// CHECK:         }
func.func @unroll_iter_args(%A: memref<16xf32>) -> f32 {
  %c0 = arith.constant 0 : index
  %c16 = arith.constant 16 : index
  %c1 = arith.constant 1 : index
  %zero = arith.constant 0.0 : f32
  %r = scf.for %i = %c0 to %c16 step %c1 iter_args(%acc = %zero) -> (f32) {
    %v = memref.load %A[%i] : memref<16xf32>
    %s = arith.addf %acc, %v : f32
    scf.yield %s : f32
  }
  return %r : f32
}

// ============================================================================
// TEST 5: GEMM-generated loop is skipped (already unrolled for the target)
// ============================================================================

// CHECK-LABEL: func.func @skip_gemm_generated
// CHECK:         scf.for
// CHECK:           memref.load
// CHECK:           memref.store
// CHECK-NOT:       memref.store
// CHECK:         }
func.func @skip_gemm_generated(%A: memref<16xf32>) {
  %c0 = arith.constant 0 : index
  %c16 = arith.constant 16 : index
  %c1 = arith.constant 1 : index
  scf.for %i = %c0 to %c16 step %c1 {
    %v = memref.load %A[%i] : memref<16xf32>
    memref.store %v, %A[%i] : memref<16xf32>
  } {trscd.gemm_generated}
  return
}
