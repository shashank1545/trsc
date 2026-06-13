// RUN: %trsc-opt --trsc-licm %s | %FileCheck %s

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
