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

// ============================================================================
// TEST 4: Do not fuse loops with different step values
// ============================================================================

// CHECK-LABEL: func.func @no_fuse_different_step
// CHECK:         scf.for
// CHECK:           "test.opA"
// CHECK:         }
// CHECK:         scf.for
// CHECK:           "test.opB"
// CHECK:         }
// CHECK:         return
func.func @no_fuse_different_step() {
  %c0 = arith.constant 0 : index
  %c100 = arith.constant 100 : index
  %c1 = arith.constant 1 : index
  %c2 = arith.constant 2 : index

  scf.for %i = %c0 to %c100 step %c1 {
    "test.opA"(%i) : (index) -> ()
  }

  scf.for %j = %c0 to %c100 step %c2 {
    "test.opB"(%j) : (index) -> ()
  }

  return
}

// ============================================================================
// TEST 5: Three consecutive fusible loops — only first pair fuses per pass run
// ============================================================================
// After fusing loops 1+2, the iterator advances past the fused loop, so loop 3
// is not fused with the result. Two loops should remain after a single pass run.

// CHECK-LABEL: func.func @fuse_three_consecutive_loops
// CHECK:         scf.for
// CHECK:           "test.opA"
// CHECK:           "test.opB"
// CHECK:         }
// CHECK:         scf.for
// CHECK:           "test.opC"
// CHECK:         }
// CHECK:         return
func.func @fuse_three_consecutive_loops() {
  %c0 = arith.constant 0 : index
  %c100 = arith.constant 100 : index
  %c1 = arith.constant 1 : index

  scf.for %i = %c0 to %c100 step %c1 {
    "test.opA"(%i) : (index) -> ()
  }

  scf.for %j = %c0 to %c100 step %c1 {
    "test.opB"(%j) : (index) -> ()
  }

  scf.for %k = %c0 to %c100 step %c1 {
    "test.opC"(%k) : (index) -> ()
  }

  return
}

// ============================================================================
// TEST 6: Do not fuse loops separated by an intervening operation
// ============================================================================

// CHECK-LABEL: func.func @no_fuse_intervening_op
// CHECK:         scf.for
// CHECK:           "test.opA"
// CHECK:         }
// CHECK:         "test.barrier"
// CHECK:         scf.for
// CHECK:           "test.opB"
// CHECK:         }
// CHECK:         return
func.func @no_fuse_intervening_op() {
  %c0 = arith.constant 0 : index
  %c100 = arith.constant 100 : index
  %c1 = arith.constant 1 : index

  scf.for %i = %c0 to %c100 step %c1 {
    "test.opA"(%i) : (index) -> ()
  }

  "test.barrier"() : () -> ()

  scf.for %j = %c0 to %c100 step %c1 {
    "test.opB"(%j) : (index) -> ()
  }

  return
}

// ============================================================================
// TEST 7: Fuse loops with WAR (write-after-read) on aligned indices
// ============================================================================
// Loop 1 reads buf[i], loop 2 writes buf[j]. Indices are perfectly aligned,
// so areIndicesPerfectlyAligned returns true and fusion is allowed.

// CHECK-LABEL: func.func @fuse_war_aligned_index
// CHECK:         scf.for %[[IV:.*]] =
// CHECK:           memref.load %{{.*}}[%[[IV]]]
// CHECK:           memref.store %{{.*}}, %{{.*}}[%[[IV]]]
// CHECK:         }
// CHECK-NOT:     scf.for
// CHECK:         return
func.func @fuse_war_aligned_index(%buf: memref<100xi32>) {
  %c0 = arith.constant 0 : index
  %c100 = arith.constant 100 : index
  %c1 = arith.constant 1 : index

  scf.for %i = %c0 to %c100 step %c1 {
    %v = memref.load %buf[%i] : memref<100xi32>
  }

  scf.for %j = %c0 to %c100 step %c1 {
    %c42 = arith.constant 42 : i32
    memref.store %c42, %buf[%j] : memref<100xi32>
  }

  return
}

// ============================================================================
// TEST 8: Fuse loops with WAW (write-after-write) on aligned indices
// ============================================================================

// CHECK-LABEL: func.func @fuse_waw_aligned_index
// CHECK:         scf.for %[[IV:.*]] =
// CHECK:           memref.store
// CHECK:           memref.store
// CHECK:         }
// CHECK-NOT:     scf.for
// CHECK:         return
func.func @fuse_waw_aligned_index(%buf: memref<100xi32>) {
  %c0 = arith.constant 0 : index
  %c100 = arith.constant 100 : index
  %c1 = arith.constant 1 : index

  scf.for %i = %c0 to %c100 step %c1 {
    %c42 = arith.constant 42 : i32
    memref.store %c42, %buf[%i] : memref<100xi32>
  }

  scf.for %j = %c0 to %c100 step %c1 {
    %c99 = arith.constant 99 : i32
    memref.store %c99, %buf[%j] : memref<100xi32>
  }

  return
}

// ============================================================================
// TEST 9: Do not fuse loops with misaligned memory indices
// ============================================================================
// Loop 1 writes buf[i], loop 2 reads buf[i+1]. The index expressions differ,
// so areIndicesPerfectlyAligned returns false and fusion is blocked.

// CHECK-LABEL: func.func @no_fuse_misaligned_indices
// CHECK:         scf.for
// CHECK:           memref.store
// CHECK:         }
// CHECK:         scf.for
// CHECK:           memref.load
// CHECK:         }
// CHECK:         return
func.func @no_fuse_misaligned_indices(%buf: memref<100xi32>) {
  %c0 = arith.constant 0 : index
  %c100 = arith.constant 100 : index
  %c1 = arith.constant 1 : index

  scf.for %i = %c0 to %c100 step %c1 {
    %c42 = arith.constant 42 : i32
    memref.store %c42, %buf[%i] : memref<100xi32>
  }

  scf.for %j = %c0 to %c100 step %c1 {
    %offset = arith.addi %j, %c1 : index
    %v = memref.load %buf[%offset] : memref<100xi32>
  }

  return
}

// ============================================================================
// TEST 10: Fuse adjacent loops nested inside an outer loop
// ============================================================================

// CHECK-LABEL: func.func @fuse_inside_outer_loop
// CHECK:         scf.for
// CHECK:           scf.for %[[IV:.*]] =
// CHECK:             "test.opA"(%[[IV]])
// CHECK:             "test.opB"(%[[IV]])
// CHECK:           }
// CHECK-NOT:       scf.for
// CHECK:         }
// CHECK:         return
func.func @fuse_inside_outer_loop() {
  %c0 = arith.constant 0 : index
  %c10 = arith.constant 10 : index
  %c100 = arith.constant 100 : index
  %c1 = arith.constant 1 : index

  scf.for %outer = %c0 to %c10 step %c1 {
    scf.for %i = %c0 to %c100 step %c1 {
      "test.opA"(%i) : (index) -> ()
    }

    scf.for %j = %c0 to %c100 step %c1 {
      "test.opB"(%j) : (index) -> ()
    }
  }

  return
}

// ============================================================================
// TEST 11: Fuse loops with multiple ops — verify body order preserved
// ============================================================================
// After fusion, loop1 ops should appear first, then loop2 ops.

// CHECK-LABEL: func.func @fuse_multi_op_bodies
// CHECK:         scf.for %[[IV:.*]] =
// CHECK-NEXT:      "test.opA1"(%[[IV]])
// CHECK-NEXT:      "test.opA2"(%[[IV]])
// CHECK-NEXT:      "test.opB1"(%[[IV]])
// CHECK-NEXT:      "test.opB2"(%[[IV]])
// CHECK-NEXT:    }
// CHECK-NEXT:    return
func.func @fuse_multi_op_bodies() {
  %c0 = arith.constant 0 : index
  %c100 = arith.constant 100 : index
  %c1 = arith.constant 1 : index

  scf.for %i = %c0 to %c100 step %c1 {
    "test.opA1"(%i) : (index) -> ()
    "test.opA2"(%i) : (index) -> ()
  }

  scf.for %j = %c0 to %c100 step %c1 {
    "test.opB1"(%j) : (index) -> ()
    "test.opB2"(%j) : (index) -> ()
  }

  return
}

// ============================================================================
// TEST 12: Fuse loops with empty bodies (only implicit scf.yield)
// ============================================================================

// CHECK-LABEL: func.func @fuse_empty_bodies
// CHECK:         scf.for
// CHECK:         }
// CHECK-NOT:     scf.for
// CHECK:         return
func.func @fuse_empty_bodies() {
  %c0 = arith.constant 0 : index
  %c100 = arith.constant 100 : index
  %c1 = arith.constant 1 : index

  scf.for %i = %c0 to %c100 step %c1 {
  }

  scf.for %j = %c0 to %c100 step %c1 {
  }

  return
}

// ============================================================================
// TEST 13: No-op — function with no loops (pass should not crash)
// ============================================================================

// CHECK-LABEL: func.func @no_loops_noop
// CHECK-NEXT:    "test.opA"() : () -> ()
// CHECK-NEXT:    return
func.func @no_loops_noop() {
  "test.opA"() : () -> ()
  return
}
