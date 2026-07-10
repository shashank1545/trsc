// RUN: %trsc-opt --gemm-epilogue-fusion %s | %FileCheck %s

// A GEMM followed by a column bias add and ReLU is represented as one
// epilogue-aware GEMM.  No separate elementwise output loop remains.
func.func @gemm_bias_relu(%a: memref<4x4xf32>, %b: memref<4x4xf32>,
                          %c: memref<4x4xf32>, %bias: memref<4xf32>) {
  trscd.gemm(%a, %b, %c) : memref<4x4xf32>, memref<4x4xf32>, memref<4x4xf32>
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c4 = arith.constant 4 : index
  %zero = arith.constant 0.0 : f32
  scf.for %i = %c0 to %c4 step %c1 {
    scf.for %j = %c0 to %c4 step %c1 {
      %value = memref.load %c[%i, %j] : memref<4x4xf32>
      %bias_value = memref.load %bias[%j] : memref<4xf32>
      %sum = arith.addf %value, %bias_value : f32
      %relu = arith.maximumf %sum, %zero : f32
      memref.store %relu, %c[%i, %j] : memref<4x4xf32>
    }
  }
  return
}

// CHECK-LABEL: func.func @gemm_bias_relu
// CHECK: trscd.gemm(%{{.*}}, %{{.*}}, %{{.*}}, %{{.*}}) {relu = true}
// CHECK-NOT: scf.for
