// RUN: %trsc-opt --allow-unregistered-dialect --trsc-licm %s | %FileCheck %s

// ============================================================================
// TEST 1: Constant computation hoisted out of loop
// ============================================================================
// A constant arith.addi inside a loop should be hoisted to the preheader.

// CHECK-LABEL: func.func @hoist_constant_computation
// CHECK-NEXT:    %[[C0:.*]] = arith.constant 0 : index
// CHECK-NEXT:    %[[C100:.*]] = arith.constant 100 : index
// CHECK-NEXT:    %[[C1:.*]] = arith.constant 1 : index
// CHECK-NEXT:    %[[C5:.*]] = arith.constant 5 : i32
// CHECK-NEXT:    %[[C10:.*]] = arith.constant 10 : i32
// CHECK-NEXT:    %[[ADD:.*]] = arith.addi %[[C5]], %[[C10]] : i32
// CHECK:         scf.for
// CHECK-NOT:       arith.addi
// CHECK:         }
// CHECK:         return
func.func @hoist_constant_computation() {
  %c0 = arith.constant 0 : index
  %c100 = arith.constant 100 : index
  %c1 = arith.constant 1 : index
  scf.for %i = %c0 to %c100 step %c1 {
    %c5 = arith.constant 5 : i32
    %c10 = arith.constant 10 : i32
    %add = arith.addi %c5, %c10 : i32
  }
  return
}

// ============================================================================
// TEST 2: Induction variable dependent code stays in loop
// ============================================================================
// An arith.addi that uses the induction variable must NOT be hoisted.

// CHECK-LABEL: func.func @keep_iv_dependent
// CHECK:         scf.for %[[IV:.*]] =
// CHECK:           arith.index_cast %[[IV]]
// CHECK:           arith.addi
// CHECK:         }
func.func @keep_iv_dependent() {
  %c0 = arith.constant 0 : index
  %c100 = arith.constant 100 : index
  %c1 = arith.constant 1 : index
  scf.for %i = %c0 to %c100 step %c1 {
    %iv = arith.index_cast %i : index to i32
    %c5 = arith.constant 5 : i32
    %add = arith.addi %iv, %c5 : i32
  }
  return
}

// ============================================================================
// TEST 3: Side-effecting ops stay in loop
// ============================================================================
// memref.store has side effects and must NOT be hoisted.

// CHECK-LABEL: func.func @keep_side_effects
// CHECK:         scf.for
// CHECK:           memref.store
// CHECK:         }
func.func @keep_side_effects(%buf: memref<100xi32>) {
  %c0 = arith.constant 0 : index
  %c100 = arith.constant 100 : index
  %c1 = arith.constant 1 : index
  %c42 = arith.constant 42 : i32
  scf.for %i = %c0 to %c100 step %c1 {
    memref.store %c42, %buf[%i] : memref<100xi32>
  }
  return
}

// ============================================================================
// TEST 4: Operand defined outside loop is hoistable
// ============================================================================
// An op whose operands are all defined BEFORE the loop should be hoisted.

// CHECK-LABEL: func.func @hoist_outside_operands
// CHECK:         %[[A:.*]] = arith.constant 3 : i32
// CHECK:         %[[B:.*]] = arith.constant 7 : i32
// CHECK:         %[[MUL:.*]] = arith.muli %[[A]], %[[B]] : i32
// CHECK:         scf.for
// CHECK-NOT:       arith.muli
// CHECK:         }
func.func @hoist_outside_operands() {
  %a = arith.constant 3 : i32
  %b = arith.constant 7 : i32
  %c0 = arith.constant 0 : index
  %c50 = arith.constant 50 : index
  %c1 = arith.constant 1 : index
  scf.for %i = %c0 to %c50 step %c1 {
    %mul = arith.muli %a, %b : i32
  }
  return
}

// ============================================================================
// TEST 5: Op dependent on another loop-body op stays in loop
// ============================================================================
// If op B depends on op A which is defined inside the loop, both must stay.

// CHECK-LABEL: func.func @keep_body_dependent
// CHECK:         scf.for %[[IV:.*]] =
// CHECK:           %[[CAST:.*]] = arith.index_cast %[[IV]]
// CHECK:           arith.muli %[[CAST]]
// CHECK:         }
func.func @keep_body_dependent() {
  %c0 = arith.constant 0 : index
  %c100 = arith.constant 100 : index
  %c1 = arith.constant 1 : index
  %c3 = arith.constant 3 : i32
  scf.for %i = %c0 to %c100 step %c1 {
    %iv = arith.index_cast %i : index to i32
    %mul = arith.muli %iv, %c3 : i32
  }
  return
}

// ============================================================================
// TEST 6: Nested loop — hoist from inner to outer body (not past outer)
// ============================================================================
// A constant op inside the inner loop should be hoisted to the outer loop body
// (which is the inner loop's preheader), not past the outer loop.

// CHECK-LABEL: func.func @nested_loop_hoist
// CHECK:       %[[C99:.*]] = arith.constant 99 : i32
// CHECK:         scf.for
// CHECK:           scf.for
// CHECK-NOT:         arith.constant 99
// CHECK:           }
// CHECK:         }
func.func @nested_loop_hoist() {
  %c0 = arith.constant 0 : index
  %c10 = arith.constant 10 : index
  %c1 = arith.constant 1 : index
  scf.for %i = %c0 to %c10 step %c1 {
    scf.for %j = %c0 to %c10 step %c1 {
      %c99 = arith.constant 99 : i32
    }
  }
  return
}

// ============================================================================
// TEST 7: Chain hoisting — fixed-point iteration hoists dependent chain
// ============================================================================
// %c5 and %c10 hoist first; then %sum (depends only on hoisted ops) hoists;
// then %prod hoists.  All four ops should end up before the loop.

// CHECK-LABEL: func.func @hoist_chain
// CHECK-DAG:     %[[C5:.*]] = arith.constant 5 : i32
// CHECK-DAG:     %[[C10:.*]] = arith.constant 10 : i32
// CHECK:         %[[SUM:.*]] = arith.addi %[[C5]], %[[C10]] : i32
// CHECK:         arith.muli %[[SUM]], %[[C10]] : i32
// CHECK:         scf.for
// CHECK-NOT:       arith.addi
// CHECK-NOT:       arith.muli
// CHECK:         }
func.func @hoist_chain() {
  %c0 = arith.constant 0 : index
  %c100 = arith.constant 100 : index
  %c1 = arith.constant 1 : index
  scf.for %i = %c0 to %c100 step %c1 {
    %c5 = arith.constant 5 : i32
    %c10 = arith.constant 10 : i32
    %sum = arith.addi %c5, %c10 : i32
    %prod = arith.muli %sum, %c10 : i32
  }
  return
}

// ============================================================================
// TEST 8: Partially hoistable chain
// ============================================================================
// %c5 is loop-invariant and hoists.  %iv depends on the induction variable
// and stays, as does %sum which depends on %iv.

// CHECK-LABEL: func.func @hoist_partial_chain
// CHECK:         arith.constant 5 : i32
// CHECK:         scf.for %[[IV:.*]] =
// CHECK:           arith.index_cast %[[IV]]
// CHECK:           arith.addi
// CHECK:         }
func.func @hoist_partial_chain() {
  %c0 = arith.constant 0 : index
  %c100 = arith.constant 100 : index
  %c1 = arith.constant 1 : index
  scf.for %i = %c0 to %c100 step %c1 {
    %c5 = arith.constant 5 : i32
    %iv = arith.index_cast %i : index to i32
    %sum = arith.addi %iv, %c5 : i32
  }
  return
}

// ============================================================================
// TEST 9: memref.load with loop-invariant address stays in loop
// ============================================================================
// Even though the index is constant, memref.load has a Read memory effect
// and isPure returns false — it must NOT be hoisted.

// CHECK-LABEL: func.func @keep_invariant_load
// CHECK:         scf.for
// CHECK:           memref.load
// CHECK:         }
func.func @keep_invariant_load(%buf: memref<100xi32>) {
  %c0 = arith.constant 0 : index
  %c100 = arith.constant 100 : index
  %c1 = arith.constant 1 : index
  %c5 = arith.constant 5 : index
  scf.for %i = %c0 to %c100 step %c1 {
    %v = memref.load %buf[%c5] : memref<100xi32>
  }
  return
}

// ============================================================================
// TEST 10: Hoistable ops inside a loop with iter_args
// ============================================================================
// The constants and their addi are loop-invariant and should hoist.
// The addi using the iter_arg (%acc) must stay inside the loop.

// CHECK-LABEL: func.func @hoist_with_iter_args
// CHECK-DAG:     %[[C5:.*]] = arith.constant 5 : i32
// CHECK-DAG:     %[[C10:.*]] = arith.constant 10 : i32
// CHECK:         %[[INV:.*]] = arith.addi %[[C5]], %[[C10]] : i32
// CHECK:         scf.for
// CHECK-NOT:       arith.constant 5 : i32
// CHECK-NOT:       arith.constant 10 : i32
// CHECK:           arith.addi %{{.*}}, %[[INV]] : i32
// CHECK:           scf.yield
// CHECK:         }
func.func @hoist_with_iter_args() -> i32 {
  %c0 = arith.constant 0 : index
  %c100 = arith.constant 100 : index
  %c1 = arith.constant 1 : index
  %c0_i32 = arith.constant 0 : i32
  %res = scf.for %i = %c0 to %c100 step %c1 iter_args(%acc = %c0_i32) -> (i32) {
    %c5 = arith.constant 5 : i32
    %c10 = arith.constant 10 : i32
    %add = arith.addi %c5, %c10 : i32
    %new_acc = arith.addi %acc, %add : i32
    scf.yield %new_acc : i32
  }
  return %res : i32
}

// ============================================================================
// TEST 11: Triple-nested loop — constant hoisted through all levels
// ============================================================================
// With post-order walk, the constant is hoisted out of the innermost loop,
// then out of the middle loop, then out of the outer loop. The side-effecting
// "test.use_*" ops anchor each loop in place.

// CHECK-LABEL: func.func @triple_nested_hoist
// CHECK:         %[[C42:.*]] = arith.constant 42 : i32
// CHECK:         scf.for
// CHECK:           "test.use_i"
// CHECK:           scf.for
// CHECK:             "test.use_j"
// CHECK:             scf.for
// CHECK-NOT:           arith.constant 42
// CHECK:               "test.use_k"
// CHECK:             }
// CHECK:           }
// CHECK:         }
func.func @triple_nested_hoist() {
  %c0 = arith.constant 0 : index
  %c10 = arith.constant 10 : index
  %c1 = arith.constant 1 : index
  scf.for %i = %c0 to %c10 step %c1 {
    "test.use_i"(%i) : (index) -> ()
    scf.for %j = %c0 to %c10 step %c1 {
      "test.use_j"(%j) : (index) -> ()
      scf.for %k = %c0 to %c10 step %c1 {
        %c42 = arith.constant 42 : i32
        "test.use_k"(%k) : (index) -> ()
      }
    }
  }
  return
}

// ============================================================================
// TEST 12: Inner-loop op depending on outer IV — hoist from inner, keep in outer
// ============================================================================
// %iv_i and %add depend on outer IV %i, so they are invariant w.r.t. the inner
// loop (hoisted to the outer body) but NOT invariant w.r.t. the outer loop.
// %c5 is fully invariant and hoists to function level.

// CHECK-LABEL: func.func @hoist_outer_iv_dep_from_inner
// CHECK:         arith.constant 5 : i32
// CHECK:         scf.for %[[I:.*]] =
// CHECK:           arith.index_cast %[[I]]
// CHECK:           arith.addi
// CHECK:           scf.for %[[J:.*]] =
// CHECK-NOT:         arith.index_cast
// CHECK-NOT:         arith.addi
// CHECK:             "test.use_j"(%[[J]])
// CHECK:           }
// CHECK:         }
func.func @hoist_outer_iv_dep_from_inner() {
  %c0 = arith.constant 0 : index
  %c10 = arith.constant 10 : index
  %c1 = arith.constant 1 : index
  scf.for %i = %c0 to %c10 step %c1 {
    scf.for %j = %c0 to %c10 step %c1 {
      %iv_i = arith.index_cast %i : index to i32
      %c5 = arith.constant 5 : i32
      %add = arith.addi %iv_i, %c5 : i32
      "test.use_j"(%j) : (index) -> ()
    }
  }
  return
}

// ============================================================================
// TEST 13: scf.if with side effects stays in loop
// ============================================================================
// The scf.if contains a side-effecting unregistered op, making it non-pure.
// It must NOT be hoisted even though %cond is loop-invariant.

// CHECK-LABEL: func.func @keep_scf_if_in_loop
// CHECK:         scf.for
// CHECK:           scf.if
// CHECK:             "test.side_effect"
// CHECK:           }
// CHECK:         }
func.func @keep_scf_if_in_loop(%cond: i1) {
  %c0 = arith.constant 0 : index
  %c100 = arith.constant 100 : index
  %c1 = arith.constant 1 : index
  scf.for %i = %c0 to %c100 step %c1 {
    scf.if %cond {
      "test.side_effect"() : () -> ()
    }
  }
  return
}

// ============================================================================
// TEST 14: Hoisting from a zero-trip loop
// ============================================================================
// The loop has zero iterations (lb == ub). Hoisting a pure op is still valid;
// it simply means the op now executes unconditionally.

// CHECK-LABEL: func.func @hoist_zero_trip_loop
// CHECK:         arith.constant 42 : i32
// CHECK:         scf.for
// CHECK-NOT:       arith.constant 42
// CHECK:         }
func.func @hoist_zero_trip_loop() {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  scf.for %i = %c0 to %c0 step %c1 {
    %c42 = arith.constant 42 : i32
  }
  return
}

// ============================================================================
// TEST 15: Multiple independent invariant ops all hoisted
// ============================================================================

// CHECK-LABEL: func.func @hoist_multiple_independent
// CHECK-DAG:     arith.constant 1 : i32
// CHECK-DAG:     arith.constant 2 : i32
// CHECK-DAG:     arith.constant 3 : i32
// CHECK-DAG:     arith.constant 4 : i32
// CHECK:         scf.for
// CHECK-NOT:       arith.constant {{[1-4]}} : i32
// CHECK:         }
func.func @hoist_multiple_independent() {
  %c0 = arith.constant 0 : index
  %c100 = arith.constant 100 : index
  %c1_idx = arith.constant 1 : index
  scf.for %i = %c0 to %c100 step %c1_idx {
    %a = arith.constant 1 : i32
    %b = arith.constant 2 : i32
    %c = arith.constant 3 : i32
    %d = arith.constant 4 : i32
  }
  return
}

// ============================================================================
// TEST 16: No-op — nothing is loop-invariant (everything uses IV)
// ============================================================================

// CHECK-LABEL: func.func @licm_noop
// CHECK:         scf.for %[[IV:.*]] =
// CHECK:           arith.index_cast %[[IV]]
// CHECK:         }
func.func @licm_noop() {
  %c0 = arith.constant 0 : index
  %c100 = arith.constant 100 : index
  %c1 = arith.constant 1 : index
  scf.for %i = %c0 to %c100 step %c1 {
    %iv = arith.index_cast %i : index to i32
  }
  return
}

// ============================================================================
// TEST 17: memref.alloc has allocation side effect — stays in loop
// ============================================================================

// CHECK-LABEL: func.func @keep_alloc_in_loop
// CHECK:         scf.for
// CHECK:           memref.alloc
// CHECK:         }
func.func @keep_alloc_in_loop() {
  %c0 = arith.constant 0 : index
  %c100 = arith.constant 100 : index
  %c1 = arith.constant 1 : index
  scf.for %i = %c0 to %c100 step %c1 {
    %buf = memref.alloc() : memref<10xi32>
  }
  return
}
