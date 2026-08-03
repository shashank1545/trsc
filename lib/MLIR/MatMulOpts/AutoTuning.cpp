#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/IR/Builders.h"
#include "mlir/Pass/Pass.h"
#include "trsc/MLIR/MatMulOpts/MatMulOptPasses.h"
#include "trsc/MLIR/TrscOps.h"

using namespace mlir;

namespace {
struct AutoTuningPass
    : public PassWrapper<AutoTuningPass, OperationPass<func::FuncOp>> {
  MLIR_DEFINE_EXPLICIT_INTERNAL_INLINE_TYPE_ID(AutoTuningPass)

  trscd::AutoTuneConfig config;

  AutoTuningPass(const trscd::AutoTuneConfig &config) : config(config) {}

  StringRef getArgument() const final { return "matmul-autotune"; }
  StringRef getDescription() const final {
    return "Compute optimal tiling parameters for trscd.gemm ops";
  }

  void runOnOperation() override {
    getOperation().walk([&](trscd::GemmOp gemmOp) {
      Builder builder(gemmOp.getContext());

      SmallVector<NamedAttribute, 10> attrs;
      // Heuristic parameters based on standard CUDA dense matmul optimization
      // (e.g. Cutlass)
      attrs.push_back(
          builder.getNamedAttr("tile_M", builder.getI64IntegerAttr(128)));
      attrs.push_back(
          builder.getNamedAttr("tile_N", builder.getI64IntegerAttr(128)));
      attrs.push_back(
          builder.getNamedAttr("tile_K", builder.getI64IntegerAttr(16)));

      attrs.push_back(
          builder.getNamedAttr("thread_tile_M", builder.getI64IntegerAttr(8)));
      attrs.push_back(
          builder.getNamedAttr("thread_tile_N", builder.getI64IntegerAttr(8)));

      attrs.push_back(
          builder.getNamedAttr("warp_tile_M", builder.getI64IntegerAttr(32)));
      attrs.push_back(
          builder.getNamedAttr("warp_tile_N", builder.getI64IntegerAttr(64)));

      attrs.push_back(
          builder.getNamedAttr("warp_sub_M", builder.getI64IntegerAttr(16)));
      attrs.push_back(
          builder.getNamedAttr("warp_sub_N", builder.getI64IntegerAttr(16)));

      attrs.push_back(builder.getNamedAttr("vectorize_width",
                                           builder.getI64IntegerAttr(4)));

      gemmOp.setTilingParamsAttr(builder.getDictionaryAttr(attrs));
    });
  }
};
} // namespace

namespace mlir {
namespace trscd {

std::unique_ptr<mlir::Pass> createAutoTuningPass(const AutoTuneConfig &config) {
  return std::make_unique<AutoTuningPass>(config);
}

} // namespace trscd
} // namespace mlir
