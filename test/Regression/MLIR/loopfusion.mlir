// RUN: %trsc-opt --allow-unregistered-dialect --trsc-loopfusion %s | %FileCheck %s

// ============================================================================
// TEST 1: Fuse two simple adjacent loops with identical bounds
// ============================================================================

// CHECK-LABEL: func.func @fuse_simple_loops
// CHECK-NEXT:    %[[C0:.*]] = arith.constant 0 : index
// CHECK-NEXT:    %[[C100:.*]] = arith.constant 100 : index
// CHECK-NEXT:    %[[C1:.*]] = arith.constant 1 : index
// CHECK-NEXT:    scf.for %[[IV:.*]] = %[[C0]] to %[[C100]] step %[[C1]] {
// CHECK-NEXT:      "test.opA"(%[[IV]]) : (index) -> ()
// CHECK-NEXT:      "test.opB"(%[[IV]]) : (index) -> ()
// CHECK-NEXT:    }
// CHECK-NEXT:    return
func.func @fuse_simple_loops() {
  %c0 = arith.constant 0 : index
  %c100 = arith.constant 100 : index
  %c1 = arith.constant 1 : index

  scf.for %i = %c0 to %c100 step %c1 {
    "test.opA"(%i) : (index) -> ()
  }

  scf.for %j = %c0 to %c100 step %c1 {
    "test.opB"(%j) : (index) -> ()
  }

  return
}

// ============================================================================
// TEST 2: Do not fuse loops with different bounds
// ============================================================================

// CHECK-LABEL: func.func @no_fuse_different_bounds
// CHECK:         scf.for
// CHECK:           "test.opA"
// CHECK:         }
// CHECK:         scf.for
// CHECK:           "test.opB"
// CHECK:         }
// CHECK:         return
func.func @no_fuse_different_bounds() {
  %c0 = arith.constant 0 : index
  %c100 = arith.constant 100 : index
  %c50 = arith.constant 50 : index
  %c1 = arith.constant 1 : index

  scf.for %i = %c0 to %c100 step %c1 {
    "test.opA"(%i) : (index) -> ()
  }

  scf.for %j = %c0 to %c50 step %c1 {
    "test.opB"(%j) : (index) -> ()
  }

  return
}

// ============================================================================
// TEST 3: Do not fuse loops that yield results (iter_args)
// ============================================================================

// CHECK-LABEL: func.func @no_fuse_iter_args
// CHECK:         scf.for
// CHECK:           "test.opA"
// CHECK:         }
// CHECK:         scf.for
// CHECK:           "test.opB"
// CHECK:         }
// CHECK:         return
func.func @no_fuse_iter_args() {
  %c0 = arith.constant 0 : index
  %c100 = arith.constant 100 : index
  %c1 = arith.constant 1 : index
  %c0_i32 = arith.constant 0 : i32

  %res1 = scf.for %i = %c0 to %c100 step %c1 iter_args(%arg = %c0_i32) -> (i32) {
    "test.opA"(%i) : (index) -> ()
    scf.yield %arg : i32
  }

  %res2 = scf.for %j = %c0 to %c100 step %c1 iter_args(%arg = %c0_i32) -> (i32) {
    "test.opB"(%j) : (index) -> ()
    scf.yield %arg : i32
  }

  return
}
