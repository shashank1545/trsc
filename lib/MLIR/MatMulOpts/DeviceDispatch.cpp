#include "trsc/MLIR/MatMulOpts/MatMulOptPasses.h"

#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Pass/Pass.h"
#include "trsc/MLIR/TrscOps.h"

using namespace mlir;

namespace {

constexpr llvm::StringLiteral kGemmTargetAttr = "trscd.gemm_target";

func::FuncOp getOrCreateRuntimeFunction(ModuleOp module, StringRef name,
                                        FunctionType type) {
  if (auto function = module.lookupSymbol<func::FuncOp>(name))
    return function;

  OpBuilder builder(module.getBodyRegion());
  auto function = func::FuncOp::create(builder, module.getLoc(), name, type);
  function.setPrivate();
  return function;
}

Value emitI32Call(OpBuilder &builder, Location loc, func::FuncOp callee,
                  ValueRange arguments = {}) {
  return func::CallOp::create(builder, loc, callee, arguments).getResult(0);
}

void emitVoidCall(OpBuilder &builder, Location loc, func::FuncOp callee) {
  func::CallOp::create(builder, loc, callee, ValueRange{});
}

Value asCondition(OpBuilder &builder, Location loc, Value value) {
  Value zero = arith::ConstantIntOp::create(builder, loc, 0, 32);
  return arith::CmpIOp::create(builder, loc, arith::CmpIPredicate::ne, value,
                              zero);
}

void tagGemm(trscd::GemmOp gemm, StringRef target) {
  gemm->setAttr(kGemmTargetAttr,
                StringAttr::get(gemm.getContext(), target));
}

trscd::GemmOp cloneGemm(OpBuilder &builder, trscd::GemmOp source,
                        Value output, StringRef target) {
  Operation *clone = builder.clone(*source.getOperation());
  clone->setOperand(2, output);
  auto gemm = cast<trscd::GemmOp>(clone);
  tagGemm(gemm, target);
  return gemm;
}

Value allocateScratchLike(OpBuilder &builder, Location loc, Value source) {
  auto type = cast<MemRefType>(source.getType());
  SmallVector<Value> dynamicSizes;
  for (int64_t dimension = 0; dimension < type.getRank(); ++dimension)
    if (type.isDynamicDim(dimension))
      dynamicSizes.push_back(
          memref::DimOp::create(builder, loc, source, dimension));
  return memref::AllocOp::create(builder, loc, type, dynamicSizes);
}

struct MaterializeDeviceDispatchPass
    : PassWrapper<MaterializeDeviceDispatchPass,
                  OperationPass<func::FuncOp>> {
  MLIR_DEFINE_EXPLICIT_INTERNAL_INLINE_TYPE_ID(MaterializeDeviceDispatchPass)

  int optLevel;
  trsc::TargetOptions target;

  MaterializeDeviceDispatchPass(int optLevel,
                                const trsc::TargetOptions &target)
      : optLevel(optLevel), target(target) {}

  StringRef getArgument() const final { return "matmul-device-dispatch"; }
  StringRef getDescription() const final {
    return "Materialize CUDA/CPU GEMM runtime dispatch";
  }

  void runOnOperation() override {
    if (optLevel < 2)
      return;

    func::FuncOp function = getOperation();
    if (target.Device == trsc::DeviceMode::CPU) {
      function.walk([&](trscd::GemmOp gemm) { tagGemm(gemm, "cpu"); });
      return;
    }

    ModuleOp module = function->getParentOfType<ModuleOp>();
    OpBuilder declarationBuilder(module.getBodyRegion());
    Type i32 = declarationBuilder.getI32Type();
    FunctionType noArgsI32 = declarationBuilder.getFunctionType({}, {i32});
    FunctionType noArgsVoid = declarationBuilder.getFunctionType({}, {});

    func::FuncOp isAvailable = getOrCreateRuntimeFunction(
        module, "trsc_cuda_is_available",
        declarationBuilder.getFunctionType({i32}, {i32}));
    func::FuncOp resetError = getOrCreateRuntimeFunction(
        module, "trsc_cuda_reset_error", noArgsVoid);
    func::FuncOp hadError = getOrCreateRuntimeFunction(
        module, "trsc_cuda_had_error", noArgsI32);
    func::FuncOp abortCuda = getOrCreateRuntimeFunction(
        module, "trsc_cuda_abort", noArgsVoid);

    SmallVector<trscd::GemmOp> work;
    function.walk([&](trscd::GemmOp gemm) { work.push_back(gemm); });

    for (trscd::GemmOp gemm : work) {
      OpBuilder builder(gemm);
      Location loc = gemm.getLoc();
      Value capability = arith::ConstantIntOp::create(
          builder, loc, target.MinimumCudaCapability, 32);
      Value available = asCondition(
          builder, loc, emitI32Call(builder, loc, isAvailable, capability));
      auto dispatch =
          scf::IfOp::create(builder, loc, available, /*withElseRegion=*/true);

      builder.setInsertionPointToStart(&dispatch.getThenRegion().front());
      emitVoidCall(builder, loc, resetError);

      if (target.Device == trsc::DeviceMode::Auto) {
        Value scratch = allocateScratchLike(builder, loc, gemm.getC());
        memref::CopyOp::create(builder, loc, gemm.getC(), scratch);
        cloneGemm(builder, gemm, scratch, "cuda");

        Value failed = asCondition(
            builder, loc, emitI32Call(builder, loc, hadError));
        auto recover =
            scf::IfOp::create(builder, loc, failed, /*withElseRegion=*/true);

        builder.setInsertionPointToStart(&recover.getThenRegion().front());
        cloneGemm(builder, gemm, gemm.getC(), "cpu");

        builder.setInsertionPointToStart(&recover.getElseRegion().front());
        memref::CopyOp::create(builder, loc, scratch, gemm.getC());

        builder.setInsertionPointAfter(recover);
        memref::DeallocOp::create(builder, loc, scratch);
      } else {
        cloneGemm(builder, gemm, gemm.getC(), "cuda");
        Value failed = asCondition(
            builder, loc, emitI32Call(builder, loc, hadError));
        auto report = scf::IfOp::create(builder, loc, failed,
                                        /*withElseRegion=*/false);
        builder.setInsertionPointToStart(&report.getThenRegion().front());
        emitVoidCall(builder, loc, abortCuda);
      }

      builder.setInsertionPointToStart(&dispatch.getElseRegion().front());
      if (target.Device == trsc::DeviceMode::Auto)
        cloneGemm(builder, gemm, gemm.getC(), "cpu");
      else
        emitVoidCall(builder, loc, abortCuda);

      gemm.erase();
    }
  }
};

} // namespace

namespace mlir::trscd {

std::unique_ptr<mlir::Pass>
createMaterializeDeviceDispatchPass(int optLevel,
                                    const trsc::TargetOptions &target) {
  return std::make_unique<MaterializeDeviceDispatchPass>(optLevel, target);
}

} // namespace mlir::trscd
