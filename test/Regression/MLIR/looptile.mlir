// RUN: %trsc-opt --trsc-loop-tile=tile-size=8 %s | %FileCheck %s
// RUN: %trsc-opt --trsc-loop-tile=tile-size=1 %s | %FileCheck %s --check-prefix=TS1

// ============================================================================
// TEST 1: Perfect 2D nest is tiled into a 4-deep nest
// ============================================================================
// Outer pair iterates tiles (step = 1 * tileSize), inner pair iterates points
// inside a tile with a minsi-clamped upper bound.

// CHECK-LABEL: func.func @tile_2d
// CHECK:         %[[C8:.*]] = arith.constant 8 : index
// CHECK:         %[[STEP_I:.*]] = arith.muli %{{.*}}, %[[C8]] : index
// CHECK:         scf.for %[[TI:.*]] = %{{.*}} to %{{.*}} step %[[STEP_I]]
// CHECK:           %[[STEP_J:.*]] = arith.muli %{{.*}}, %[[C8]] : index
// CHECK:           scf.for %[[TJ:.*]] = %{{.*}} to %{{.*}} step %[[STEP_J]]
// CHECK:             %[[END_I:.*]] = arith.addi %[[TI]], %[[STEP_I]] : index
// CHECK:             %[[UB_I:.*]] = arith.minsi %{{.*}}, %[[END_I]] : index
// CHECK:             scf.for %[[I:.*]] = %[[TI]] to %[[UB_I]]
// CHECK:               %[[END_J:.*]] = arith.addi %[[TJ]], %[[STEP_J]] : index
// CHECK:               %[[UB_J:.*]] = arith.minsi %{{.*}}, %[[END_J]] : index
// CHECK:               scf.for %[[J:.*]] = %[[TJ]] to %[[UB_J]]
// CHECK:                 memref.load %{{.*}}[%[[I]], %[[J]]]
// CHECK:                 memref.store

// TS1-LABEL: func.func @tile_2d
// TS1-NOT:     arith.minsi
func.func @tile_2d(%A: memref<64x64xf32>) {
  %c0 = arith.constant 0 : index
  %c64 = arith.constant 64 : index
  %c1 = arith.constant 1 : index
  scf.for %i = %c0 to %c64 step %c1 {
    scf.for %j = %c0 to %c64 step %c1 {
      %v = memref.load %A[%i, %j] : memref<64x64xf32>
      memref.store %v, %A[%j, %i] : memref<64x64xf32>
    }
  }
  return
}

// ============================================================================
// TEST 2: Perfect 3D nest is tiled into a 6-deep nest
// ============================================================================

// CHECK-LABEL: func.func @tile_3d
// CHECK:         scf.for
// CHECK:           scf.for
// CHECK:             scf.for
// CHECK:               scf.for
// CHECK:                 scf.for
// CHECK:                   scf.for
// CHECK:                     memref.store

// TS1-LABEL: func.func @tile_3d
// TS1-NOT:     arith.minsi
func.func @tile_3d(%A: memref<64x64x64xf32>) {
  %c0 = arith.constant 0 : index
  %c64 = arith.constant 64 : index
  %c1 = arith.constant 1 : index
  %cst = arith.constant 1.0 : f32
  scf.for %i = %c0 to %c64 step %c1 {
    scf.for %j = %c0 to %c64 step %c1 {
      scf.for %k = %c0 to %c64 step %c1 {
        memref.store %cst, %A[%i, %j, %k] : memref<64x64x64xf32>
      }
    }
  }
  return
}

// ============================================================================
// TEST 3: Single loop is NOT tiled (strip-mining alone buys nothing)
// ============================================================================

// CHECK-LABEL: func.func @no_tile_single_loop
// CHECK-NOT:     arith.minsi
// CHECK:         scf.for
// CHECK-NOT:       scf.for
// CHECK:         return
func.func @no_tile_single_loop(%A: memref<64xf32>) {
  %c0 = arith.constant 0 : index
  %c64 = arith.constant 64 : index
  %c1 = arith.constant 1 : index
  scf.for %i = %c0 to %c64 step %c1 {
    %v = memref.load %A[%i] : memref<64xf32>
    memref.store %v, %A[%i] : memref<64xf32>
  }
  return
}

// ============================================================================
// TEST 4: Imperfect nest is NOT tiled (code between the loops breaks the band)
// ============================================================================

// CHECK-LABEL: func.func @no_tile_imperfect_nest
// CHECK-NOT:     arith.minsi
// CHECK:         return
func.func @no_tile_imperfect_nest(%A: memref<64x64xf32>) {
  %c0 = arith.constant 0 : index
  %c64 = arith.constant 64 : index
  %c1 = arith.constant 1 : index
  scf.for %i = %c0 to %c64 step %c1 {
    %d = memref.load %A[%i, %c0] : memref<64x64xf32>
    scf.for %j = %c0 to %c64 step %c1 {
      memref.store %d, %A[%i, %j] : memref<64x64xf32>
    }
  }
  return
}

// ============================================================================
// TEST 5: GEMM-generated nest is skipped (already tiled for the target)
// ============================================================================

// CHECK-LABEL: func.func @skip_gemm_generated
// CHECK-NOT:     arith.minsi
// CHECK:         return
func.func @skip_gemm_generated(%A: memref<64x64xf32>) {
  %c0 = arith.constant 0 : index
  %c64 = arith.constant 64 : index
  %c1 = arith.constant 1 : index
  scf.for %i = %c0 to %c64 step %c1 {
    scf.for %j = %c0 to %c64 step %c1 {
      %v = memref.load %A[%i, %j] : memref<64x64xf32>
      memref.store %v, %A[%j, %i] : memref<64x64xf32>
    }
  } {trscd.gemm_generated}
  return
}
