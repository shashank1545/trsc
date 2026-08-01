#ifndef TRSC_MLIR_MATMULOPTS_MATMULOPTPASSES_H
#define TRSC_MLIR_MATMULOPTS_MATMULOPTPASSES_H

#include "mlir/Pass/Pass.h"
#include <memory>
#include <optional>
#include <string>

#include "trsc/Basic/TargetOptions.h"

namespace mlir {
namespace trscd {

// Configuration for the AutoTuning pass
struct AutoTuneConfig {
  enum class Strategy { Heuristic, CompileTimeSearch, RuntimeJIT };
  Strategy strategy = Strategy::CompileTimeSearch;
  int maxVariants = 8;
  std::optional<std::string> targetArch;
};

// Pass to recognize matmul patterns and lower them to trscd.gemm
std::unique_ptr<mlir::Pass> createMatMulRecognitionPass();

// Fuse a column bias-add and ReLU consumer into the producer GEMM's epilogue.
std::unique_ptr<mlir::Pass> createGemmEpilogueFusionPass();

// Pass to compute optimal parameters and attach them as attributes to
// trscd.gemm
std::unique_ptr<mlir::Pass> createAutoTuningPass(AutoTuneConfig config = {});

// Pass to lower trscd.gemm into standard optimized MLIR (scf, memref, gpu,
// vector) based on the optimization level
std::unique_ptr<mlir::Pass>
createMaterializeDeviceDispatchPass(int optLevel,
                                    const trsc::TargetOptions &target);

std::unique_ptr<mlir::Pass>
createLowerTrscdMatMulPass(int optLevel, trsc::DeviceMode device);

// Pipeline builder to compose the passes
void buildMatMulOptPipeline(mlir::PassManager &pm, int optLevel,
                            const trsc::TargetOptions &target,
                            AutoTuneConfig tuneConfig = {});

// Register standalone matmul optimization passes with an MLIR tool driver.
void registerMatMulOptPasses();

} // namespace trscd
} // namespace mlir

#endif // TRSC_MLIR_MATMULOPTS_MATMULOPTPASSES_H
