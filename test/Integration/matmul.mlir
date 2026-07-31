module {
  func.func @main() -> f32 {
    %c0 = arith.constant 0 : index
    %cst = arith.constant 0.000000e+00 : f32
    %cst_0 = arith.constant 2.000000e+00 : f32
    %cst_1 = arith.constant 1.000000e+00 : f32
    %alloc = memref.alloc() : memref<32x32xf32>
    %alloc_2 = memref.alloc() : memref<32x32xf32>
    %alloc_3 = memref.alloc() : memref<32x32xf32>
    linalg.fill ins(%cst_1 : f32) outs(%alloc_3 : memref<32x32xf32>)
    linalg.fill ins(%cst_0 : f32) outs(%alloc_2 : memref<32x32xf32>)
    linalg.fill ins(%cst : f32) outs(%alloc : memref<32x32xf32>)
    %0 = memref.load %alloc[%c0, %c0] : memref<32x32xf32>
    return %0 : f32
  }
}
