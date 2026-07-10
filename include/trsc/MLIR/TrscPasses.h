#ifndef TRSC_MLIR_TRSCPASSES_H 
#define TRSC_MLIR_TRSCPASSES_H 

#include "mlir/Pass/Pass.h"
#include "llvm/ADT/StringRef.h"
#include <memory>

namespace mlir {
  namespace trscd {

    /// Marker attribute attached by GEMM lowering to the loop nests and GPU
    /// launches it generates. Generic loop passes (tiling, unrolling) must
    /// not rewrite code carrying this marker.
    inline constexpr llvm::StringLiteral kGemmGeneratedMarker =
        "trscd.gemm_generated";

    /// Returns true if `op` or any of its ancestors carries the GEMM
    /// generated marker.
    inline bool isGemmGenerated(Operation *op) {
      for (; op; op = op->getParentOp())
        if (op->hasAttr(kGemmGeneratedMarker))
          return true;
      return false;
    }

    std::unique_ptr<mlir::Pass> createTrscLICM();
    std::unique_ptr<mlir::Pass> createTrscLoopFusion();
    std::unique_ptr<mlir::Pass> createTrscLoopTile();
    std::unique_ptr<mlir::Pass> createTrscLoopUnroll();
    std::unique_ptr<mlir::Pass> optimizeStandard();

    void registerTrscPasses();
    void registerOptStdPasses();

    #define GEN_PASS_DECL
    #include "TrscPasses.h.inc"

  } // namespace trscd
} // namespace mlir
#endif // TRSC_MLIR_TRSCPASSES_H 
