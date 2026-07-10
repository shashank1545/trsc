#include "mlir/Dialect/Arith/IR/Arith.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/Dialect/GPU/IR/GPUDialect.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/IR/SCF.h"
#include "mlir/Dialect/Vector/IR/VectorOps.h"
#include "mlir/IR/Builders.h"
#include "mlir/Pass/Pass.h"
#include "trsc/MLIR/MatMulOpts/MatMulOptPasses.h"
#include "trsc/MLIR/TrscOps.h"
#include "trsc/MLIR/TrscPasses.h"

#include <cstdlib>

using namespace mlir;

namespace {
// Tile-shape override for autotuning sweeps: TRSC_GEMM_TILES=
// "BM,BN,BK,TM,TN[,WM,WN,WNITER]". A tiling_params attr on the gemm op
// still wins over the environment; absent both, each level's defaults
// apply.
struct TileOverride {
  bool valid = false;
  int64_t BM, BN, BK, TM, TN;
  bool hasWarp = false;
  int64_t WM, WN, WNITER;
};

static TileOverride readTileOverride() {
  TileOverride t;
  const char *env = std::getenv("TRSC_GEMM_TILES");
  if (!env)
    return t;
  SmallVector<int64_t, 8> vals;
  StringRef s(env);
  while (!s.empty()) {
    auto [head, rest] = s.split(',');
    int64_t v;
    if (head.trim().getAsInteger(10, v))
      return t;
    vals.push_back(v);
    s = rest;
  }
  if (vals.size() != 5 && vals.size() != 8)
    return t;
  t.valid = true;
  t.BM = vals[0]; t.BN = vals[1]; t.BK = vals[2];
  t.TM = vals[3]; t.TN = vals[4];
  if (vals.size() == 8) {
    t.hasWarp = true;
    t.WM = vals[5]; t.WN = vals[6]; t.WNITER = vals[7];
  }
  return t;
}

struct LowerTrscdMatMulPass
    : public PassWrapper<LowerTrscdMatMulPass, OperationPass<func::FuncOp>> {
  MLIR_DEFINE_EXPLICIT_INTERNAL_INLINE_TYPE_ID(LowerTrscdMatMulPass)

  int optLevel;

  LowerTrscdMatMulPass(int optLevel) : optLevel(optLevel) {}

  StringRef getArgument() const final { return "matmul-lower"; }
  StringRef getDescription() const final {
    return "Lower trscd.gemm to standard MLIR based on opt level";
  }

  void runOnOperation() override {
    getOperation().walk([&](trscd::GemmOp gemmOp) {
      OpBuilder builder(gemmOp);
      Location loc = gemmOp.getLoc();

      Value A = gemmOp.getA();
      Value B = gemmOp.getB();
      Value C = gemmOp.getC();
      Value bias = gemmOp.getBias();
      bool relu = gemmOp.getRelu();

      // Keep bias-add and ReLU in the accumulator's final store.  This is the
      // epilogue selected by GemmEpilogueFusionPass and avoids a second pass
      // over C after GEMM.
      auto applyEpilogue = [&](Value value, Value column) -> Value {
        if (bias) {
          Value biasValue =
              memref::LoadOp::create(builder, loc, bias, ValueRange{column});
          value = arith::AddFOp::create(builder, loc, value, biasValue);
        }
        if (relu) {
          Value zero = arith::ConstantOp::create(
              builder, loc, builder.getF32FloatAttr(0.0f));
          value = arith::MaximumFOp::create(builder, loc, value, zero);
        }
        return value;
      };

      // GPU levels stage operands in device memory. Zero-copy pinned host
      // memory makes every kernel access cross PCIe (~6 GB/s vs ~128 GB/s
      // GDDR), which caps all kernels at PCIe bandwidth regardless of tiling.
      bool useDevice = optLevel >= 2 && optLevel <= 8;
      // gpu.memcpy/gpu.dealloc only lower to runtime calls in async form
      // (isAsyncWithOneDependency), so chain them on a token and gpu.wait.
      Type tokenType = gpu::AsyncTokenType::get(builder.getContext());
      Value hostC;
      if (useDevice) {
        Value token =
            gpu::WaitOp::create(builder, loc, tokenType, ValueRange{})
                .getAsyncToken();
        auto stageOnDevice = [&](Value host) -> Value {
          auto type = cast<MemRefType>(host.getType());
          SmallVector<Value> dynSizes;
          for (int64_t i = 0, e = type.getRank(); i < e; ++i)
            if (type.isDynamicDim(i))
              dynSizes.push_back(memref::DimOp::create(builder, loc, host, i));
          auto allocOp =
              gpu::AllocOp::create(builder, loc, type, tokenType,
                                   ValueRange{token}, dynSizes,
                                   /*symbolOperands=*/ValueRange{});
          token = allocOp.getAsyncToken();
          auto cpyOp = gpu::MemcpyOp::create(builder, loc, tokenType,
                                             ValueRange{token},
                                             allocOp.getMemref(), host);
          token = cpyOp.getAsyncToken();
          return allocOp.getMemref();
        };
        hostC = C;
        A = stageOnDevice(A);
        B = stageOnDevice(B);
        C = stageOnDevice(C);
        if (bias)
          bias = stageOnDevice(bias);
        gpu::WaitOp::create(builder, loc, /*asyncToken=*/Type(),
                            ValueRange{token});
      }

      // Copy C back to the host buffer and free device staging after the
      // launch. Each GPU branch calls this with the insertion point after its
      // gpu.launch.
      auto finishDevice = [&]() {
        if (!useDevice)
          return;
        Value token =
            gpu::WaitOp::create(builder, loc, tokenType, ValueRange{})
                .getAsyncToken();
        token = gpu::MemcpyOp::create(builder, loc, tokenType,
                                      ValueRange{token}, hostC, C)
                    .getAsyncToken();
        token = gpu::DeallocOp::create(builder, loc, tokenType,
                                       ValueRange{token}, A)
                    .getAsyncToken();
        token = gpu::DeallocOp::create(builder, loc, tokenType,
                                       ValueRange{token}, B)
                    .getAsyncToken();
        token = gpu::DeallocOp::create(builder, loc, tokenType,
                                       ValueRange{token}, C)
                    .getAsyncToken();
        if (bias)
          token = gpu::DeallocOp::create(builder, loc, tokenType,
                                         ValueRange{token}, bias)
                      .getAsyncToken();
        gpu::WaitOp::create(builder, loc, /*asyncToken=*/Type(),
                            ValueRange{token});
      };

      if (optLevel == 1) {
        Value M = memref::DimOp::create(builder, loc, A, 0);
        Value K = memref::DimOp::create(builder, loc, A, 1);
        Value N = memref::DimOp::create(builder, loc, B, 1);

        Value zero = arith::ConstantIndexOp::create(builder, loc, 0);
        Value one = arith::ConstantIndexOp::create(builder, loc, 1);

        auto loopI = scf::ForOp::create(builder, loc, zero, M, one);
        loopI->setAttr(trscd::kGemmGeneratedMarker, builder.getUnitAttr());
        builder.setInsertionPointToStart(loopI.getBody());

        auto loopJ = scf::ForOp::create(builder, loc, zero, N, one);
        builder.setInsertionPointToStart(loopJ.getBody());

        Value loadC = memref::LoadOp::create(
            builder, loc, C,
            ValueRange{loopI.getInductionVar(), loopJ.getInductionVar()});

        auto loopK =
            scf::ForOp::create(builder, loc, zero, K, one, ValueRange{loadC});
        if (!loopK.getBody()->empty()) {
          loopK.getBody()->getTerminator()->erase();
        }
        builder.setInsertionPointToStart(loopK.getBody());

        Value loadA = memref::LoadOp::create(
            builder, loc, A,
            ValueRange{loopI.getInductionVar(), loopK.getInductionVar()});
        Value loadB = memref::LoadOp::create(
            builder, loc, B,
            ValueRange{loopK.getInductionVar(), loopJ.getInductionVar()});

        Value mul = arith::MulFOp::create(builder, loc, loadA, loadB);
        Value add =
            arith::AddFOp::create(builder, loc, loopK.getRegionIterArg(0), mul);

        scf::YieldOp::create(builder, loc, ValueRange{add});

        builder.setInsertionPointAfter(loopK);
        memref::StoreOp::create(
            builder, loc,
            applyEpilogue(loopK.getResult(0), loopJ.getInductionVar()), C,
            ValueRange{loopI.getInductionVar(), loopJ.getInductionVar()});

        gemmOp.erase();
      } else if (optLevel == 2) {
        // Level 2: GMEM coalescing (gpu.launch with tx mapped to contiguous
        // memory)
        Value M = memref::DimOp::create(builder, loc, A, 0);
        Value K = memref::DimOp::create(builder, loc, A, 1);
        Value N = memref::DimOp::create(builder, loc, B, 1);

        Value zero = arith::ConstantIndexOp::create(builder, loc, 0);
        Value one = arith::ConstantIndexOp::create(builder, loc, 1);

        // Tile sizes (default to 32x32 for coalescing blocks)
        int64_t tileX = 32; // mapped to columns (N)
        int64_t tileY = 32; // mapped to rows (M)
        if (auto dictAttr =
                gemmOp->getAttrOfType<DictionaryAttr>("tiling_params")) {
          if (auto attr = llvm::dyn_cast_or_null<IntegerAttr>(
                  dictAttr.get("thread_tile_N")))
            tileX = attr.getInt();
          if (auto attr = llvm::dyn_cast_or_null<IntegerAttr>(
                  dictAttr.get("thread_tile_M")))
            tileY = attr.getInt();
        }

        Value blockDimX = arith::ConstantIndexOp::create(builder, loc, tileX);
        Value blockDimY = arith::ConstantIndexOp::create(builder, loc, tileY);

        // gridDimX = (N + blockDimX - 1) / blockDimX
        Value nMinus1 = arith::SubIOp::create(builder, loc, N, one);
        Value nAdd = arith::AddIOp::create(builder, loc, nMinus1, blockDimX);
        Value gridDimX = arith::DivUIOp::create(builder, loc, nAdd, blockDimX);

        // gridDimY = (M + blockDimY - 1) / blockDimY
        Value mMinus1 = arith::SubIOp::create(builder, loc, M, one);
        Value mAdd = arith::AddIOp::create(builder, loc, mMinus1, blockDimY);
        Value gridDimY = arith::DivUIOp::create(builder, loc, mAdd, blockDimY);

        auto launchOp = gpu::LaunchOp::create(builder, loc, gridDimX, gridDimY,
                                              one, blockDimX, blockDimY, one);
        launchOp->setAttr(trscd::kGemmGeneratedMarker,
                          builder.getUnitAttr());

        builder.setInsertionPointToStart(&launchOp.getBody().front());

        Value bx = launchOp.getBlockIds().x;
        Value by = launchOp.getBlockIds().y;
        Value tx = launchOp.getThreadIds().x;
        Value ty = launchOp.getThreadIds().y;

        // col = bx * blockDimX + tx (contiguous dimension)
        Value bxMul = arith::MulIOp::create(builder, loc, bx, blockDimX);
        Value col = arith::AddIOp::create(builder, loc, bxMul, tx);

        // row = by * blockDimY + ty
        Value byMul = arith::MulIOp::create(builder, loc, by, blockDimY);
        Value row = arith::AddIOp::create(builder, loc, byMul, ty);

        // Bounds check: if (row < M && col < N)
        Value rowCond = arith::CmpIOp::create(
            builder, loc, arith::CmpIPredicate::ult, row, M);
        Value colCond = arith::CmpIOp::create(
            builder, loc, arith::CmpIPredicate::ult, col, N);
        Value cond = arith::AndIOp::create(builder, loc, rowCond, colCond);

        auto ifOp =
            scf::IfOp::create(builder, loc, cond, /*withElseRegion=*/false);
        builder.setInsertionPointToStart(&ifOp.getThenRegion().front());

        // Inner sequential loop over K
        auto loopK = scf::ForOp::create(builder, loc, zero, K, one);
        builder.setInsertionPointToStart(loopK.getBody());

        Value loadA = memref::LoadOp::create(
            builder, loc, A, ValueRange{row, loopK.getInductionVar()});
        Value loadB = memref::LoadOp::create(
            builder, loc, B, ValueRange{loopK.getInductionVar(), col});
        Value loadC =
            memref::LoadOp::create(builder, loc, C, ValueRange{row, col});

        Value mul = arith::MulFOp::create(builder, loc, loadA, loadB);
        Value add = arith::AddFOp::create(builder, loc, loadC, mul);

        memref::StoreOp::create(builder, loc, applyEpilogue(add, col), C,
                                ValueRange{row, col});

        builder.setInsertionPointAfter(ifOp);
        gpu::TerminatorOp::create(builder, loc);

        builder.setInsertionPointAfter(launchOp);
        finishDevice();
        gemmOp.erase();
      } else if (optLevel == 3) {
        // Level 3: SMEM caching
        Value M = memref::DimOp::create(builder, loc, A, 0);
        Value K = memref::DimOp::create(builder, loc, A, 1);
        Value N = memref::DimOp::create(builder, loc, B, 1);

        Value zero = arith::ConstantIndexOp::create(builder, loc, 0);
        Value one = arith::ConstantIndexOp::create(builder, loc, 1);

        int64_t tileX = 32; // mapped to N (columns)
        int64_t tileY = 32; // mapped to M (rows)
        int64_t tileK = 32; // mapped to K
        if (auto dictAttr =
                gemmOp->getAttrOfType<DictionaryAttr>("tiling_params")) {
          if (auto attr = llvm::dyn_cast_or_null<IntegerAttr>(
                  dictAttr.get("thread_tile_N")))
            tileX = attr.getInt();
          if (auto attr = llvm::dyn_cast_or_null<IntegerAttr>(
                  dictAttr.get("thread_tile_M")))
            tileY = attr.getInt();
          if (auto attr =
                  llvm::dyn_cast_or_null<IntegerAttr>(dictAttr.get("tile_K")))
            tileK = attr.getInt();
        }

        Value blockDimX = arith::ConstantIndexOp::create(builder, loc, tileX);
        Value blockDimY = arith::ConstantIndexOp::create(builder, loc, tileY);
        Value blockDimK = arith::ConstantIndexOp::create(builder, loc, tileK);

        // gridDimX = (N + blockDimX - 1) / blockDimX
        Value nMinus1 = arith::SubIOp::create(builder, loc, N, one);
        Value nAdd = arith::AddIOp::create(builder, loc, nMinus1, blockDimX);
        Value gridDimX = arith::DivUIOp::create(builder, loc, nAdd, blockDimX);

        // gridDimY = (M + blockDimY - 1) / blockDimY
        Value mMinus1 = arith::SubIOp::create(builder, loc, M, one);
        Value mAdd = arith::AddIOp::create(builder, loc, mMinus1, blockDimY);
        Value gridDimY = arith::DivUIOp::create(builder, loc, mAdd, blockDimY);

        auto launchOp = gpu::LaunchOp::create(builder, loc, gridDimX, gridDimY,
                                              one, blockDimX, blockDimY, one);
        launchOp->setAttr(trscd::kGemmGeneratedMarker,
                          builder.getUnitAttr());

        builder.setInsertionPointToStart(&launchOp.getBody().front());

        // Shared memory allocations
        auto smemSpace = gpu::AddressSpaceAttr::get(
            builder.getContext(), gpu::AddressSpace::Workgroup);
        auto f32Type = builder.getF32Type();
        MemRefType smemAType = MemRefType::get(
            {tileY, tileK}, f32Type, MemRefLayoutAttrInterface(), smemSpace);
        MemRefType smemBType = MemRefType::get(
            {tileK, tileX}, f32Type, MemRefLayoutAttrInterface(), smemSpace);

        // Workgroup attributions become static shared memory after kernel
        // outlining; memref.alloc in workgroup space would lower to a bogus
        // device-side malloc.
        Value smemA = launchOp.addWorkgroupAttribution(smemAType, loc);
        Value smemB = launchOp.addWorkgroupAttribution(smemBType, loc);

        Value bx = launchOp.getBlockIds().x;
        Value by = launchOp.getBlockIds().y;
        Value tx = launchOp.getThreadIds().x;
        Value ty = launchOp.getThreadIds().y;

        Value bxMul = arith::MulIOp::create(builder, loc, bx, blockDimX);
        Value col = arith::AddIOp::create(builder, loc, bxMul, tx);

        Value byMul = arith::MulIOp::create(builder, loc, by, blockDimY);
        Value row = arith::AddIOp::create(builder, loc, byMul, ty);

        // We load C once into a register (SSA value)
        Value rowCond = arith::CmpIOp::create(
            builder, loc, arith::CmpIPredicate::ult, row, M);
        Value colCond = arith::CmpIOp::create(
            builder, loc, arith::CmpIPredicate::ult, col, N);
        Value inBounds = arith::AndIOp::create(builder, loc, rowCond, colCond);

        auto loadCIf = scf::IfOp::create(builder, loc, f32Type, inBounds,
                                         /*withElseRegion=*/true);
        builder.setInsertionPointToStart(&loadCIf.getThenRegion().front());
        Value cVal =
            memref::LoadOp::create(builder, loc, C, ValueRange{row, col});
        scf::YieldOp::create(builder, loc, cVal);
        builder.setInsertionPointToStart(&loadCIf.getElseRegion().front());
        Value zeroF32 = arith::ConstantOp::create(builder, loc,
                                                  builder.getF32FloatAttr(0.0));
        scf::YieldOp::create(builder, loc, zeroF32);

        builder.setInsertionPointAfter(loadCIf);
        Value acc = loadCIf.getResult(0);

        // Loop over K in steps of tileK
        auto loopK = scf::ForOp::create(builder, loc, zero, K, blockDimK,
                                        ValueRange{acc});
        if (!loopK.getBody()->empty()) {
          loopK.getBody()->getTerminator()->erase();
        }
        builder.setInsertionPointToStart(loopK.getBody());
        Value currentAcc = loopK.getRegionIterArg(0);
        Value bk = loopK.getInductionVar();

        // Cooperative load A into smemA: A[row, bk + tx]
        Value bkPlusTx = arith::AddIOp::create(builder, loc, bk, tx);
        Value aColCond = arith::CmpIOp::create(
            builder, loc, arith::CmpIPredicate::ult, bkPlusTx, K);
        Value aCond = arith::AndIOp::create(builder, loc, rowCond, aColCond);

        auto loadAIf =
            scf::IfOp::create(builder, loc, aCond, /*withElseRegion=*/false);
        builder.setInsertionPointToStart(&loadAIf.getThenRegion().front());
        Value aVal =
            memref::LoadOp::create(builder, loc, A, ValueRange{row, bkPlusTx});
        memref::StoreOp::create(builder, loc, aVal, smemA, ValueRange{ty, tx});
        builder.setInsertionPointAfter(loadAIf);

        // Cooperative load B into smemB: B[bk + ty, col]
        Value bkPlusTy = arith::AddIOp::create(builder, loc, bk, ty);
        Value bRowCond = arith::CmpIOp::create(
            builder, loc, arith::CmpIPredicate::ult, bkPlusTy, K);
        Value bCond = arith::AndIOp::create(builder, loc, bRowCond, colCond);

        auto loadBIf =
            scf::IfOp::create(builder, loc, bCond, /*withElseRegion=*/false);
        builder.setInsertionPointToStart(&loadBIf.getThenRegion().front());
        Value bVal =
            memref::LoadOp::create(builder, loc, B, ValueRange{bkPlusTy, col});
        memref::StoreOp::create(builder, loc, bVal, smemB, ValueRange{ty, tx});
        builder.setInsertionPointAfter(loadBIf);

        // Sync threads before computing
        gpu::BarrierOp::create(builder, loc);

        // Compute inner loop over shared memory (tileK iterations)
        auto innerLoop = scf::ForOp::create(builder, loc, zero, blockDimK, one,
                                            ValueRange{currentAcc});
        if (!innerLoop.getBody()->empty()) {
          innerLoop.getBody()->getTerminator()->erase();
        }
        builder.setInsertionPointToStart(innerLoop.getBody());
        Value innerAcc = innerLoop.getRegionIterArg(0);
        Value kInner = innerLoop.getInductionVar();

        Value sA =
            memref::LoadOp::create(builder, loc, smemA, ValueRange{ty, kInner});
        Value sB =
            memref::LoadOp::create(builder, loc, smemB, ValueRange{kInner, tx});
        Value sMul = arith::MulFOp::create(builder, loc, sA, sB);
        Value sAdd = arith::AddFOp::create(builder, loc, innerAcc, sMul);
        scf::YieldOp::create(builder, loc, ValueRange{sAdd});

        builder.setInsertionPointAfter(innerLoop);

        // Sync threads before next block iteration to avoid overwriting shared
        // memory
        gpu::BarrierOp::create(builder, loc);
        scf::YieldOp::create(builder, loc, ValueRange{innerLoop.getResult(0)});

        builder.setInsertionPointAfter(loopK);

        // Store the result
        auto storeCIf =
            scf::IfOp::create(builder, loc, inBounds, /*withElseRegion=*/false);
        builder.setInsertionPointToStart(&storeCIf.getThenRegion().front());
        memref::StoreOp::create(builder, loc,
                                applyEpilogue(loopK.getResult(0), col), C,
                                ValueRange{row, col});

        builder.setInsertionPointAfter(storeCIf);
        gpu::TerminatorOp::create(builder, loc);

        builder.setInsertionPointAfter(launchOp);
        finishDevice();
        gemmOp.erase();
      } else if (optLevel == 4) {
        // Level 4: 1D Blocktiling
        Value M = memref::DimOp::create(builder, loc, A, 0);
        Value K = memref::DimOp::create(builder, loc, A, 1);
        Value N = memref::DimOp::create(builder, loc, B, 1);

        Value zero = arith::ConstantIndexOp::create(builder, loc, 0);
        Value one = arith::ConstantIndexOp::create(builder, loc, 1);

        int64_t BM = 64;
        int64_t BN = 64;
        int64_t BK = 8;
        int64_t TM = 8;

        if (auto dictAttr =
                gemmOp->getAttrOfType<DictionaryAttr>("tiling_params")) {
          if (auto attr =
                  llvm::dyn_cast_or_null<IntegerAttr>(dictAttr.get("tile_M")))
            BM = attr.getInt();
          if (auto attr =
                  llvm::dyn_cast_or_null<IntegerAttr>(dictAttr.get("tile_N")))
            BN = attr.getInt();
          if (auto attr =
                  llvm::dyn_cast_or_null<IntegerAttr>(dictAttr.get("tile_K")))
            BK = attr.getInt();
          if (auto attr = llvm::dyn_cast_or_null<IntegerAttr>(
                  dictAttr.get("thread_tile_M")))
            TM = attr.getInt();
        }

        Value valBM = arith::ConstantIndexOp::create(builder, loc, BM);
        Value valBN = arith::ConstantIndexOp::create(builder, loc, BN);
        Value valBK = arith::ConstantIndexOp::create(builder, loc, BK);
        Value valTM = arith::ConstantIndexOp::create(builder, loc, TM);

        Value blockDimX = valBN;
        Value blockDimY = arith::ConstantIndexOp::create(builder, loc, BM / TM);

        // gridDimX = (N + BN - 1) / BN
        Value nMinus1 = arith::SubIOp::create(builder, loc, N, one);
        Value nAdd = arith::AddIOp::create(builder, loc, nMinus1, valBN);
        Value gridDimX = arith::DivUIOp::create(builder, loc, nAdd, valBN);

        // gridDimY = (M + BM - 1) / BM
        Value mMinus1 = arith::SubIOp::create(builder, loc, M, one);
        Value mAdd = arith::AddIOp::create(builder, loc, mMinus1, valBM);
        Value gridDimY = arith::DivUIOp::create(builder, loc, mAdd, valBM);

        auto launchOp = gpu::LaunchOp::create(builder, loc, gridDimX, gridDimY,
                                              one, blockDimX, blockDimY, one);
        launchOp->setAttr(trscd::kGemmGeneratedMarker,
                          builder.getUnitAttr());

        builder.setInsertionPointToStart(&launchOp.getBody().front());

        auto smemSpace = gpu::AddressSpaceAttr::get(
            builder.getContext(), gpu::AddressSpace::Workgroup);
        auto f32Type = builder.getF32Type();
        MemRefType smemAType = MemRefType::get(
            {BM, BK}, f32Type, MemRefLayoutAttrInterface(), smemSpace);
        MemRefType smemBType = MemRefType::get(
            {BK, BN}, f32Type, MemRefLayoutAttrInterface(), smemSpace);

        // Workgroup attributions become static shared memory after kernel
        // outlining; memref.alloc in workgroup space would lower to a bogus
        // device-side malloc.
        Value smemA = launchOp.addWorkgroupAttribution(smemAType, loc);
        Value smemB = launchOp.addWorkgroupAttribution(smemBType, loc);

        Value bx = launchOp.getBlockIds().x;
        Value by = launchOp.getBlockIds().y;
        Value tx = launchOp.getThreadIds().x;
        Value ty = launchOp.getThreadIds().y;

        Value col = arith::AddIOp::create(
            builder, loc, arith::MulIOp::create(builder, loc, bx, valBN), tx);
        Value threadRowStart = arith::AddIOp::create(
            builder, loc, arith::MulIOp::create(builder, loc, by, valBM),
            arith::MulIOp::create(builder, loc, ty, valTM));

        // Accumulators live in SSA values threaded through the K loop as
        // iter_args. A memref.alloca indexed by loop induction vars lowers to
        // off-chip local memory (ld.local/st.local in the FMA loop), so the
        // thread tile is fully unrolled with compile-time indices instead.
        Value tyTM = arith::MulIOp::create(builder, loc, ty, valTM);
        SmallVector<Value> cIdx;
        for (int64_t t = 0; t < TM; ++t)
          cIdx.push_back(arith::ConstantIndexOp::create(builder, loc, t));
        Value zeroF32 = arith::ConstantOp::create(builder, loc,
                                                  builder.getF32FloatAttr(0.0));
        Value colCondInit = arith::CmpIOp::create(
            builder, loc, arith::CmpIPredicate::ult, col, N);

        // Initialize accumulators from C
        SmallVector<Value> initAcc;
        initAcc.reserve(TM);
        for (int64_t i = 0; i < TM; ++i) {
          Value rowInit =
              arith::AddIOp::create(builder, loc, threadRowStart, cIdx[i]);
          Value rowCondInit = arith::CmpIOp::create(
              builder, loc, arith::CmpIPredicate::ult, rowInit, M);
          Value inBoundsInit =
              arith::AndIOp::create(builder, loc, rowCondInit, colCondInit);
          auto loadCIf = scf::IfOp::create(builder, loc, f32Type, inBoundsInit,
                                           /*withElseRegion=*/true);
          builder.setInsertionPointToStart(&loadCIf.getThenRegion().front());
          Value cVal =
              memref::LoadOp::create(builder, loc, C, ValueRange{rowInit, col});
          scf::YieldOp::create(builder, loc, cVal);
          builder.setInsertionPointToStart(&loadCIf.getElseRegion().front());
          scf::YieldOp::create(builder, loc, zeroF32);
          builder.setInsertionPointAfter(loadCIf);
          initAcc.push_back(loadCIf.getResult(0));
        }

        // Outer K loop carrying the accumulators
        auto loopK = scf::ForOp::create(builder, loc, zero, K, valBK, initAcc);
        if (!loopK.getBody()->empty()) {
          loopK.getBody()->getTerminator()->erase();
        }
        builder.setInsertionPointToStart(loopK.getBody());
        Value bk = loopK.getInductionVar();

        // Cooperative load A into smemA. A block has (BM / TM) * BN threads,
        // which is usually equal to BM * BK, but let's just do a linear map for
        // simplicity: tid = ty * blockDimX + tx.
        Value tid = arith::AddIOp::create(
            builder, loc, arith::MulIOp::create(builder, loc, ty, blockDimX),
            tx);

        // We might need a loop if threads < BM*BK
        int64_t threadsPerBlock = (BM / TM) * BN;
        Value valThreads =
            arith::ConstantIndexOp::create(builder, loc, threadsPerBlock);
        Value bmTimesBk = arith::ConstantIndexOp::create(builder, loc, BM * BK);

        auto loadALoop =
            scf::ForOp::create(builder, loc, tid, bmTimesBk, valThreads);
        builder.setInsertionPointToStart(loadALoop.getBody());
        Value linearA = loadALoop.getInductionVar();
        Value aRow = arith::DivUIOp::create(builder, loc, linearA, valBK);
        Value aCol = arith::RemUIOp::create(builder, loc, linearA, valBK);

        Value globalARow = arith::AddIOp::create(
            builder, loc, arith::MulIOp::create(builder, loc, by, valBM), aRow);
        Value globalACol = arith::AddIOp::create(builder, loc, bk, aCol);

        Value aRowCond = arith::CmpIOp::create(
            builder, loc, arith::CmpIPredicate::ult, globalARow, M);
        Value aColCond = arith::CmpIOp::create(
            builder, loc, arith::CmpIPredicate::ult, globalACol, K);
        Value aCond = arith::AndIOp::create(builder, loc, aRowCond, aColCond);

        auto loadAIf =
            scf::IfOp::create(builder, loc, aCond, /*withElseRegion=*/false);
        builder.setInsertionPointToStart(&loadAIf.getThenRegion().front());
        Value aVal = memref::LoadOp::create(builder, loc, A,
                                            ValueRange{globalARow, globalACol});
        memref::StoreOp::create(builder, loc, aVal, smemA,
                                ValueRange{aRow, aCol});
        builder.setInsertionPointAfter(loadAIf);
        builder.setInsertionPointAfter(loadALoop);

        // Cooperative load B into smemB.
        Value bkTimesBn = arith::ConstantIndexOp::create(builder, loc, BK * BN);
        auto loadBLoop =
            scf::ForOp::create(builder, loc, tid, bkTimesBn, valThreads);
        builder.setInsertionPointToStart(loadBLoop.getBody());
        Value linearB = loadBLoop.getInductionVar();
        Value bRow = arith::DivUIOp::create(builder, loc, linearB, valBN);
        Value bCol = arith::RemUIOp::create(builder, loc, linearB, valBN);

        Value globalBRow = arith::AddIOp::create(builder, loc, bk, bRow);
        Value globalBCol = arith::AddIOp::create(
            builder, loc, arith::MulIOp::create(builder, loc, bx, valBN), bCol);

        Value bRowCond = arith::CmpIOp::create(
            builder, loc, arith::CmpIPredicate::ult, globalBRow, K);
        Value bColCond = arith::CmpIOp::create(
            builder, loc, arith::CmpIPredicate::ult, globalBCol, N);
        Value bCond = arith::AndIOp::create(builder, loc, bRowCond, bColCond);

        auto loadBIf =
            scf::IfOp::create(builder, loc, bCond, /*withElseRegion=*/false);
        builder.setInsertionPointToStart(&loadBIf.getThenRegion().front());
        Value bVal = memref::LoadOp::create(builder, loc, B,
                                            ValueRange{globalBRow, globalBCol});
        memref::StoreOp::create(builder, loc, bVal, smemB,
                                ValueRange{bRow, bCol});
        builder.setInsertionPointAfter(loadBIf);
        builder.setInsertionPointAfter(loadBLoop);

        gpu::BarrierOp::create(builder, loc);

        // Inner loop over K; TM FMAs unrolled on SSA accumulators
        auto innerLoop = scf::ForOp::create(builder, loc, zero, valBK, one,
                                            loopK.getRegionIterArgs());
        if (!innerLoop.getBody()->empty()) {
          innerLoop.getBody()->getTerminator()->erase();
        }
        builder.setInsertionPointToStart(innerLoop.getBody());
        Value kInner = innerLoop.getInductionVar();

        Value sB =
            memref::LoadOp::create(builder, loc, smemB, ValueRange{kInner, tx});

        SmallVector<Value> nextAcc;
        nextAcc.reserve(TM);
        for (int64_t i = 0; i < TM; ++i) {
          Value aSmRow = arith::AddIOp::create(builder, loc, tyTM, cIdx[i]);
          Value sA = memref::LoadOp::create(builder, loc, smemA,
                                            ValueRange{aSmRow, kInner});
          Value sMul = arith::MulFOp::create(builder, loc, sA, sB);
          nextAcc.push_back(arith::AddFOp::create(
              builder, loc, innerLoop.getRegionIterArg(i), sMul));
        }
        scf::YieldOp::create(builder, loc, nextAcc);

        builder.setInsertionPointAfter(innerLoop);

        gpu::BarrierOp::create(builder, loc);
        scf::YieldOp::create(builder, loc, innerLoop.getResults());

        builder.setInsertionPointAfter(loopK);

        // Store results (unrolled)
        Value sColCond = arith::CmpIOp::create(
            builder, loc, arith::CmpIPredicate::ult, col, N);
        for (int64_t i = 0; i < TM; ++i) {
          Value sRow =
              arith::AddIOp::create(builder, loc, threadRowStart, cIdx[i]);
          Value sRowCond = arith::CmpIOp::create(
              builder, loc, arith::CmpIPredicate::ult, sRow, M);
          Value sInBounds =
              arith::AndIOp::create(builder, loc, sRowCond, sColCond);
          auto storeCIf = scf::IfOp::create(builder, loc, sInBounds,
                                            /*withElseRegion=*/false);
          builder.setInsertionPointToStart(&storeCIf.getThenRegion().front());
          memref::StoreOp::create(builder, loc,
                                  applyEpilogue(loopK.getResult(i), col), C,
                                  ValueRange{sRow, col});
          builder.setInsertionPointAfter(storeCIf);
        }

        gpu::TerminatorOp::create(builder, loc);

        builder.setInsertionPointAfter(launchOp);
        finishDevice();
        gemmOp.erase();
      } else if (optLevel == 5) {
        // Level 5: 2D Blocktiling
        Value M = memref::DimOp::create(builder, loc, A, 0);
        Value K = memref::DimOp::create(builder, loc, A, 1);
        Value N = memref::DimOp::create(builder, loc, B, 1);

        Value zero = arith::ConstantIndexOp::create(builder, loc, 0);
        Value one = arith::ConstantIndexOp::create(builder, loc, 1);

        int64_t BM = 64;
        int64_t BN = 64;
        int64_t BK = 8;
        int64_t TM = 8;
        int64_t TN = 8;

        if (TileOverride ov = readTileOverride(); ov.valid) {
          BM = ov.BM; BN = ov.BN; BK = ov.BK; TM = ov.TM; TN = ov.TN;
        }

        if (auto dictAttr =
                gemmOp->getAttrOfType<DictionaryAttr>("tiling_params")) {
          if (auto attr =
                  llvm::dyn_cast_or_null<IntegerAttr>(dictAttr.get("tile_M")))
            BM = attr.getInt();
          if (auto attr =
                  llvm::dyn_cast_or_null<IntegerAttr>(dictAttr.get("tile_N")))
            BN = attr.getInt();
          if (auto attr =
                  llvm::dyn_cast_or_null<IntegerAttr>(dictAttr.get("tile_K")))
            BK = attr.getInt();
          if (auto attr = llvm::dyn_cast_or_null<IntegerAttr>(
                  dictAttr.get("thread_tile_M")))
            TM = attr.getInt();
          if (auto attr = llvm::dyn_cast_or_null<IntegerAttr>(
                  dictAttr.get("thread_tile_N")))
            TN = attr.getInt();
        }

        Value valBM = arith::ConstantIndexOp::create(builder, loc, BM);
        Value valBN = arith::ConstantIndexOp::create(builder, loc, BN);
        Value valBK = arith::ConstantIndexOp::create(builder, loc, BK);
        Value valTM = arith::ConstantIndexOp::create(builder, loc, TM);
        Value valTN = arith::ConstantIndexOp::create(builder, loc, TN);

        Value blockDimX = arith::ConstantIndexOp::create(builder, loc, BN / TN);
        Value blockDimY = arith::ConstantIndexOp::create(builder, loc, BM / TM);

        // gridDimX = (N + BN - 1) / BN
        Value nMinus1 = arith::SubIOp::create(builder, loc, N, one);
        Value nAdd = arith::AddIOp::create(builder, loc, nMinus1, valBN);
        Value gridDimX = arith::DivUIOp::create(builder, loc, nAdd, valBN);

        // gridDimY = (M + BM - 1) / BM
        Value mMinus1 = arith::SubIOp::create(builder, loc, M, one);
        Value mAdd = arith::AddIOp::create(builder, loc, mMinus1, valBM);
        Value gridDimY = arith::DivUIOp::create(builder, loc, mAdd, valBM);

        auto launchOp = gpu::LaunchOp::create(builder, loc, gridDimX, gridDimY,
                                              one, blockDimX, blockDimY, one);
        launchOp->setAttr(trscd::kGemmGeneratedMarker,
                          builder.getUnitAttr());

        builder.setInsertionPointToStart(&launchOp.getBody().front());

        auto smemSpace = gpu::AddressSpaceAttr::get(
            builder.getContext(), gpu::AddressSpace::Workgroup);
        auto f32Type = builder.getF32Type();
        MemRefType smemAType = MemRefType::get(
            {BM, BK}, f32Type, MemRefLayoutAttrInterface(), smemSpace);
        MemRefType smemBType = MemRefType::get(
            {BK, BN}, f32Type, MemRefLayoutAttrInterface(), smemSpace);

        // Workgroup attributions become static shared memory after kernel
        // outlining; memref.alloc in workgroup space would lower to a bogus
        // device-side malloc.
        Value smemA = launchOp.addWorkgroupAttribution(smemAType, loc);
        Value smemB = launchOp.addWorkgroupAttribution(smemBType, loc);

        Value bx = launchOp.getBlockIds().x;
        Value by = launchOp.getBlockIds().y;
        Value tx = launchOp.getThreadIds().x;
        Value ty = launchOp.getThreadIds().y;

        Value threadColStart = arith::AddIOp::create(
            builder, loc, arith::MulIOp::create(builder, loc, bx, valBN),
            arith::MulIOp::create(builder, loc, tx, valTN));
        Value threadRowStart = arith::AddIOp::create(
            builder, loc, arith::MulIOp::create(builder, loc, by, valBM),
            arith::MulIOp::create(builder, loc, ty, valTM));

        // Accumulators live in SSA values threaded through the K loop as
        // iter_args. memref.alloca register arrays indexed by loop induction
        // vars lower to off-chip local memory (ld.local/st.local in the FMA
        // loop), so the TMxTN thread tile is fully unrolled with compile-time
        // indices instead.
        Value tyTM = arith::MulIOp::create(builder, loc, ty, valTM);
        Value txTN = arith::MulIOp::create(builder, loc, tx, valTN);
        int64_t maxT = TM > TN ? TM : TN;
        SmallVector<Value> cIdx;
        for (int64_t t = 0; t < maxT; ++t)
          cIdx.push_back(arith::ConstantIndexOp::create(builder, loc, t));
        Value zeroF32 = arith::ConstantOp::create(builder, loc,
                                                  builder.getF32FloatAttr(0.0));

        // Initialize accumulators from C
        SmallVector<Value> initAcc;
        initAcc.reserve(TM * TN);
        for (int64_t i = 0; i < TM; ++i) {
          Value rowInit =
              arith::AddIOp::create(builder, loc, threadRowStart, cIdx[i]);
          Value rowCondInit = arith::CmpIOp::create(
              builder, loc, arith::CmpIPredicate::ult, rowInit, M);
          for (int64_t j = 0; j < TN; ++j) {
            Value colInit =
                arith::AddIOp::create(builder, loc, threadColStart, cIdx[j]);
            Value colCondInit = arith::CmpIOp::create(
                builder, loc, arith::CmpIPredicate::ult, colInit, N);
            Value inBoundsInit =
                arith::AndIOp::create(builder, loc, rowCondInit, colCondInit);
            auto loadCIf = scf::IfOp::create(builder, loc, f32Type,
                                             inBoundsInit,
                                             /*withElseRegion=*/true);
            builder.setInsertionPointToStart(&loadCIf.getThenRegion().front());
            Value cVal = memref::LoadOp::create(builder, loc, C,
                                                ValueRange{rowInit, colInit});
            scf::YieldOp::create(builder, loc, cVal);
            builder.setInsertionPointToStart(&loadCIf.getElseRegion().front());
            scf::YieldOp::create(builder, loc, zeroF32);
            builder.setInsertionPointAfter(loadCIf);
            initAcc.push_back(loadCIf.getResult(0));
          }
        }

        // Outer K loop carrying the accumulators
        auto loopK = scf::ForOp::create(builder, loc, zero, K, valBK, initAcc);
        if (!loopK.getBody()->empty()) {
          loopK.getBody()->getTerminator()->erase();
        }
        builder.setInsertionPointToStart(loopK.getBody());
        Value bk = loopK.getInductionVar();

        // Cooperative load A into smemA. A block has (BM / TM) * (BN / TN)
        // threads.
        Value tid = arith::AddIOp::create(
            builder, loc, arith::MulIOp::create(builder, loc, ty, blockDimX),
            tx);

        int64_t threadsPerBlock = (BM / TM) * (BN / TN);
        Value valThreads =
            arith::ConstantIndexOp::create(builder, loc, threadsPerBlock);

        Value bmTimesBk = arith::ConstantIndexOp::create(builder, loc, BM * BK);
        auto loadALoop =
            scf::ForOp::create(builder, loc, tid, bmTimesBk, valThreads);
        builder.setInsertionPointToStart(loadALoop.getBody());
        Value linearA = loadALoop.getInductionVar();
        Value aRow = arith::DivUIOp::create(builder, loc, linearA, valBK);
        Value aCol = arith::RemUIOp::create(builder, loc, linearA, valBK);

        Value globalARow = arith::AddIOp::create(
            builder, loc, arith::MulIOp::create(builder, loc, by, valBM), aRow);
        Value globalACol = arith::AddIOp::create(builder, loc, bk, aCol);

        Value aRowCond = arith::CmpIOp::create(
            builder, loc, arith::CmpIPredicate::ult, globalARow, M);
        Value aColCond = arith::CmpIOp::create(
            builder, loc, arith::CmpIPredicate::ult, globalACol, K);
        Value aCond = arith::AndIOp::create(builder, loc, aRowCond, aColCond);

        auto loadAIf =
            scf::IfOp::create(builder, loc, aCond, /*withElseRegion=*/false);
        builder.setInsertionPointToStart(&loadAIf.getThenRegion().front());
        Value aVal = memref::LoadOp::create(builder, loc, A,
                                            ValueRange{globalARow, globalACol});
        memref::StoreOp::create(builder, loc, aVal, smemA,
                                ValueRange{aRow, aCol});
        builder.setInsertionPointAfter(loadAIf);
        builder.setInsertionPointAfter(loadALoop);

        // Cooperative load B into smemB.
        Value bkTimesBn = arith::ConstantIndexOp::create(builder, loc, BK * BN);
        auto loadBLoop =
            scf::ForOp::create(builder, loc, tid, bkTimesBn, valThreads);
        builder.setInsertionPointToStart(loadBLoop.getBody());
        Value linearB = loadBLoop.getInductionVar();
        Value bRow = arith::DivUIOp::create(builder, loc, linearB, valBN);
        Value bCol = arith::RemUIOp::create(builder, loc, linearB, valBN);

        Value globalBRow = arith::AddIOp::create(builder, loc, bk, bRow);
        Value globalBCol = arith::AddIOp::create(
            builder, loc, arith::MulIOp::create(builder, loc, bx, valBN), bCol);

        Value bRowCond = arith::CmpIOp::create(
            builder, loc, arith::CmpIPredicate::ult, globalBRow, K);
        Value bColCond = arith::CmpIOp::create(
            builder, loc, arith::CmpIPredicate::ult, globalBCol, N);
        Value bCond = arith::AndIOp::create(builder, loc, bRowCond, bColCond);

        auto loadBIf =
            scf::IfOp::create(builder, loc, bCond, /*withElseRegion=*/false);
        builder.setInsertionPointToStart(&loadBIf.getThenRegion().front());
        Value bVal = memref::LoadOp::create(builder, loc, B,
                                            ValueRange{globalBRow, globalBCol});
        memref::StoreOp::create(builder, loc, bVal, smemB,
                                ValueRange{bRow, bCol});
        builder.setInsertionPointAfter(loadBIf);
        builder.setInsertionPointAfter(loadBLoop);

        gpu::BarrierOp::create(builder, loc);

        // Inner loop over BK; TMxTN FMAs unrolled on SSA values
        auto innerLoop = scf::ForOp::create(builder, loc, zero, valBK, one,
                                            loopK.getRegionIterArgs());
        if (!innerLoop.getBody()->empty()) {
          innerLoop.getBody()->getTerminator()->erase();
        }
        builder.setInsertionPointToStart(innerLoop.getBody());
        Value kInner = innerLoop.getInductionVar();

        // Per-thread fragments of the SMEM tiles (registers)
        SmallVector<Value> regA, regB;
        for (int64_t i = 0; i < TM; ++i) {
          Value aSmRow = arith::AddIOp::create(builder, loc, tyTM, cIdx[i]);
          regA.push_back(memref::LoadOp::create(builder, loc, smemA,
                                                ValueRange{aSmRow, kInner}));
        }
        for (int64_t j = 0; j < TN; ++j) {
          Value bSmCol = arith::AddIOp::create(builder, loc, txTN, cIdx[j]);
          regB.push_back(memref::LoadOp::create(builder, loc, smemB,
                                                ValueRange{kInner, bSmCol}));
        }

        SmallVector<Value> nextAcc;
        nextAcc.reserve(TM * TN);
        for (int64_t i = 0; i < TM; ++i) {
          for (int64_t j = 0; j < TN; ++j) {
            Value sMul = arith::MulFOp::create(builder, loc, regA[i], regB[j]);
            nextAcc.push_back(arith::AddFOp::create(
                builder, loc, innerLoop.getRegionIterArg(i * TN + j), sMul));
          }
        }
        scf::YieldOp::create(builder, loc, nextAcc);

        builder.setInsertionPointAfter(innerLoop);

        gpu::BarrierOp::create(builder, loc);
        scf::YieldOp::create(builder, loc, innerLoop.getResults());

        builder.setInsertionPointAfter(loopK);

        // Store results (unrolled)
        for (int64_t i = 0; i < TM; ++i) {
          Value sRow =
              arith::AddIOp::create(builder, loc, threadRowStart, cIdx[i]);
          Value sRowCond = arith::CmpIOp::create(
              builder, loc, arith::CmpIPredicate::ult, sRow, M);
          for (int64_t j = 0; j < TN; ++j) {
            Value sCol =
                arith::AddIOp::create(builder, loc, threadColStart, cIdx[j]);
            Value sColCond = arith::CmpIOp::create(
                builder, loc, arith::CmpIPredicate::ult, sCol, N);
            Value sInBounds =
                arith::AndIOp::create(builder, loc, sRowCond, sColCond);
            auto storeCIf = scf::IfOp::create(builder, loc, sInBounds,
                                              /*withElseRegion=*/false);
            builder.setInsertionPointToStart(
                &storeCIf.getThenRegion().front());
            memref::StoreOp::create(
                builder, loc, applyEpilogue(loopK.getResult(i * TN + j), sCol),
                                    C, ValueRange{sRow, sCol});
            builder.setInsertionPointAfter(storeCIf);
          }
        }

        gpu::TerminatorOp::create(builder, loc);

        builder.setInsertionPointAfter(launchOp);
        finishDevice();
        gemmOp.erase();
      } else if (optLevel == 6) {
        // Level 6: Vectorization. float4 GMEM loads; A is stored transposed
        // in SMEM (BK x BM) so per-thread A fragments are vectorizable too.
        Value M = memref::DimOp::create(builder, loc, A, 0);
        Value K = memref::DimOp::create(builder, loc, A, 1);
        Value N = memref::DimOp::create(builder, loc, B, 1);

        Value zero = arith::ConstantIndexOp::create(builder, loc, 0);
        Value one = arith::ConstantIndexOp::create(builder, loc, 1);

        int64_t BM = 64;
        int64_t BN = 64;
        int64_t BK = 8;
        int64_t TM = 8;
        int64_t TN = 8;

        if (TileOverride ov = readTileOverride(); ov.valid) {
          BM = ov.BM; BN = ov.BN; BK = ov.BK; TM = ov.TM; TN = ov.TN;
        }

        if (auto dictAttr =
                gemmOp->getAttrOfType<DictionaryAttr>("tiling_params")) {
          if (auto attr =
                  llvm::dyn_cast_or_null<IntegerAttr>(dictAttr.get("tile_M")))
            BM = attr.getInt();
          if (auto attr =
                  llvm::dyn_cast_or_null<IntegerAttr>(dictAttr.get("tile_N")))
            BN = attr.getInt();
          if (auto attr =
                  llvm::dyn_cast_or_null<IntegerAttr>(dictAttr.get("tile_K")))
            BK = attr.getInt();
          if (auto attr = llvm::dyn_cast_or_null<IntegerAttr>(
                  dictAttr.get("thread_tile_M")))
            TM = attr.getInt();
          if (auto attr = llvm::dyn_cast_or_null<IntegerAttr>(
                  dictAttr.get("thread_tile_N")))
            TN = attr.getInt();
        }

        Value valBM = arith::ConstantIndexOp::create(builder, loc, BM);
        Value valBN = arith::ConstantIndexOp::create(builder, loc, BN);
        Value valBK = arith::ConstantIndexOp::create(builder, loc, BK);
        Value valTM = arith::ConstantIndexOp::create(builder, loc, TM);
        Value valTN = arith::ConstantIndexOp::create(builder, loc, TN);

        Value blockDimX = arith::ConstantIndexOp::create(builder, loc, BN / TN);
        Value blockDimY = arith::ConstantIndexOp::create(builder, loc, BM / TM);

        // gridDimX = (N + BN - 1) / BN
        Value nMinus1 = arith::SubIOp::create(builder, loc, N, one);
        Value nAdd = arith::AddIOp::create(builder, loc, nMinus1, valBN);
        Value gridDimX = arith::DivUIOp::create(builder, loc, nAdd, valBN);

        // gridDimY = (M + BM - 1) / BM
        Value mMinus1 = arith::SubIOp::create(builder, loc, M, one);
        Value mAdd = arith::AddIOp::create(builder, loc, mMinus1, valBM);
        Value gridDimY = arith::DivUIOp::create(builder, loc, mAdd, valBM);

        auto launchOp = gpu::LaunchOp::create(builder, loc, gridDimX, gridDimY,
                                              one, blockDimX, blockDimY, one);
        launchOp->setAttr(trscd::kGemmGeneratedMarker,
                          builder.getUnitAttr());

        builder.setInsertionPointToStart(&launchOp.getBody().front());

        auto smemSpace = gpu::AddressSpaceAttr::get(
            builder.getContext(), gpu::AddressSpace::Workgroup);
        auto f32Type = builder.getF32Type();
        auto vec4Type = VectorType::get({4}, f32Type);

        // smemA transposed: rows are k-slices, contiguous along BM.
        MemRefType smemAType = MemRefType::get(
            {BK, BM}, f32Type, MemRefLayoutAttrInterface(), smemSpace);
        MemRefType smemBType = MemRefType::get(
            {BK, BN}, f32Type, MemRefLayoutAttrInterface(), smemSpace);

        // Workgroup attributions become static shared memory after kernel
        // outlining; memref.alloc in workgroup space would lower to a bogus
        // device-side malloc.
        Value smemA = launchOp.addWorkgroupAttribution(smemAType, loc);
        Value smemB = launchOp.addWorkgroupAttribution(smemBType, loc);

        Value bx = launchOp.getBlockIds().x;
        Value by = launchOp.getBlockIds().y;
        Value tx = launchOp.getThreadIds().x;
        Value ty = launchOp.getThreadIds().y;

        Value threadColStart = arith::AddIOp::create(
            builder, loc, arith::MulIOp::create(builder, loc, bx, valBN),
            arith::MulIOp::create(builder, loc, tx, valTN));
        Value threadRowStart = arith::AddIOp::create(
            builder, loc, arith::MulIOp::create(builder, loc, by, valBM),
            arith::MulIOp::create(builder, loc, ty, valTM));

        // Accumulators live in SSA values threaded through the K loop as
        // iter_args. memref.alloca register arrays indexed by loop induction
        // vars lower to off-chip local memory (ld.local/st.local in the FMA
        // loop), so the TMxTN thread tile is fully unrolled with compile-time
        // indices instead.
        Value tyTM = arith::MulIOp::create(builder, loc, ty, valTM);
        Value txTN = arith::MulIOp::create(builder, loc, tx, valTN);
        int64_t maxT = TM > TN ? TM : TN;
        if (maxT < 4)
          maxT = 4;
        SmallVector<Value> cIdx;
        for (int64_t t = 0; t < maxT; ++t)
          cIdx.push_back(arith::ConstantIndexOp::create(builder, loc, t));
        Value zeroF32 = arith::ConstantOp::create(builder, loc,
                                                  builder.getF32FloatAttr(0.0));

        // Initialize accumulators from C
        SmallVector<Value> initAcc;
        initAcc.reserve(TM * TN);
        for (int64_t i = 0; i < TM; ++i) {
          Value rowInit =
              arith::AddIOp::create(builder, loc, threadRowStart, cIdx[i]);
          Value rowCondInit = arith::CmpIOp::create(
              builder, loc, arith::CmpIPredicate::ult, rowInit, M);
          for (int64_t j = 0; j < TN; ++j) {
            Value colInit =
                arith::AddIOp::create(builder, loc, threadColStart, cIdx[j]);
            Value colCondInit = arith::CmpIOp::create(
                builder, loc, arith::CmpIPredicate::ult, colInit, N);
            Value inBoundsInit =
                arith::AndIOp::create(builder, loc, rowCondInit, colCondInit);
            auto loadCIf = scf::IfOp::create(builder, loc, f32Type,
                                             inBoundsInit,
                                             /*withElseRegion=*/true);
            builder.setInsertionPointToStart(&loadCIf.getThenRegion().front());
            Value cVal = memref::LoadOp::create(builder, loc, C,
                                                ValueRange{rowInit, colInit});
            scf::YieldOp::create(builder, loc, cVal);
            builder.setInsertionPointToStart(&loadCIf.getElseRegion().front());
            scf::YieldOp::create(builder, loc, zeroF32);
            builder.setInsertionPointAfter(loadCIf);
            initAcc.push_back(loadCIf.getResult(0));
          }
        }

        // Outer K loop carrying the accumulators
        auto loopK = scf::ForOp::create(builder, loc, zero, K, valBK, initAcc);
        if (!loopK.getBody()->empty()) {
          loopK.getBody()->getTerminator()->erase();
        }
        builder.setInsertionPointToStart(loopK.getBody());
        Value bk = loopK.getInductionVar();

        Value val4 = arith::ConstantIndexOp::create(builder, loc, 4);

        // Cooperative load A into smemA. A block has (BM / TM) * (BN / TN)
        // threads.
        Value tid = arith::AddIOp::create(
            builder, loc, arith::MulIOp::create(builder, loc, ty, blockDimX),
            tx);

        int64_t threadsPerBlock = (BM / TM) * (BN / TN);
        Value valThreads =
            arith::ConstantIndexOp::create(builder, loc, threadsPerBlock);

        Value valBKDiv4 = arith::ConstantIndexOp::create(builder, loc, BK / 4);
        Value bmTimesBkDiv4 =
            arith::ConstantIndexOp::create(builder, loc, (BM * BK) / 4);

        auto loadALoop =
            scf::ForOp::create(builder, loc, tid, bmTimesBkDiv4, valThreads);
        builder.setInsertionPointToStart(loadALoop.getBody());
        Value linearA = loadALoop.getInductionVar();
        Value aRow = arith::DivUIOp::create(builder, loc, linearA, valBKDiv4);
        Value aColDiv4 =
            arith::RemUIOp::create(builder, loc, linearA, valBKDiv4);
        Value aCol = arith::MulIOp::create(builder, loc, aColDiv4, val4);

        Value globalARow = arith::AddIOp::create(
            builder, loc, arith::MulIOp::create(builder, loc, by, valBM), aRow);
        Value globalACol = arith::AddIOp::create(builder, loc, bk, aCol);

        Value aRowCond = arith::CmpIOp::create(
            builder, loc, arith::CmpIPredicate::ult, globalARow, M);
        // Assumes M, N, K are padding-friendly (divisible by 4) so we just
        // check row and roughly col
        Value aColCond = arith::CmpIOp::create(
            builder, loc, arith::CmpIPredicate::ult, globalACol, K);
        Value aCond = arith::AndIOp::create(builder, loc, aRowCond, aColCond);

        auto loadAIf =
            scf::IfOp::create(builder, loc, aCond, /*withElseRegion=*/false);
        builder.setInsertionPointToStart(&loadAIf.getThenRegion().front());

        // Vectorized GMEM read, transposed scatter into smemA[k][m]
        Value aVecVal = vector::LoadOp::create(
            builder, loc, vec4Type, A, ValueRange{globalARow, globalACol});
        for (int64_t q = 0; q < 4; ++q) {
          Value elem = vector::ExtractOp::create(builder, loc, aVecVal, q);
          Value smRow = arith::AddIOp::create(builder, loc, aCol, cIdx[q]);
          memref::StoreOp::create(builder, loc, elem, smemA,
                                  ValueRange{smRow, aRow});
        }

        builder.setInsertionPointAfter(loadAIf);
        builder.setInsertionPointAfter(loadALoop);

        // Cooperative load B into smemB.
        Value valBNDiv4 = arith::ConstantIndexOp::create(builder, loc, BN / 4);
        Value bkTimesBnDiv4 =
            arith::ConstantIndexOp::create(builder, loc, (BK * BN) / 4);

        auto loadBLoop =
            scf::ForOp::create(builder, loc, tid, bkTimesBnDiv4, valThreads);
        builder.setInsertionPointToStart(loadBLoop.getBody());
        Value linearB = loadBLoop.getInductionVar();
        Value bRow = arith::DivUIOp::create(builder, loc, linearB, valBNDiv4);
        Value bColDiv4 =
            arith::RemUIOp::create(builder, loc, linearB, valBNDiv4);
        Value bCol = arith::MulIOp::create(builder, loc, bColDiv4, val4);

        Value globalBRow = arith::AddIOp::create(builder, loc, bk, bRow);
        Value globalBCol = arith::AddIOp::create(
            builder, loc, arith::MulIOp::create(builder, loc, bx, valBN), bCol);

        Value bRowCond = arith::CmpIOp::create(
            builder, loc, arith::CmpIPredicate::ult, globalBRow, K);
        Value bColCond = arith::CmpIOp::create(
            builder, loc, arith::CmpIPredicate::ult, globalBCol, N);
        Value bCond = arith::AndIOp::create(builder, loc, bRowCond, bColCond);

        auto loadBIf =
            scf::IfOp::create(builder, loc, bCond, /*withElseRegion=*/false);
        builder.setInsertionPointToStart(&loadBIf.getThenRegion().front());

        Value bVecVal = vector::LoadOp::create(
            builder, loc, vec4Type, B, ValueRange{globalBRow, globalBCol});
        vector::StoreOp::create(builder, loc, bVecVal, smemB,
                                ValueRange{bRow, bCol});

        builder.setInsertionPointAfter(loadBIf);
        builder.setInsertionPointAfter(loadBLoop);

        gpu::BarrierOp::create(builder, loc);

        // Inner loop over BK; TMxTN FMAs unrolled on SSA values
        auto innerLoop = scf::ForOp::create(builder, loc, zero, valBK, one,
                                            loopK.getRegionIterArgs());
        if (!innerLoop.getBody()->empty()) {
          innerLoop.getBody()->getTerminator()->erase();
        }
        builder.setInsertionPointToStart(innerLoop.getBody());
        Value kInner = innerLoop.getInductionVar();

        // Per-thread fragments (registers). smemA row kInner is contiguous
        // along BM thanks to the transpose, so fragments load as float4.
        SmallVector<Value> regA, regB;
        if (TM % 4 == 0) {
          for (int64_t i = 0; i < TM; i += 4) {
            Value off = arith::AddIOp::create(builder, loc, tyTM, cIdx[i]);
            Value v = vector::LoadOp::create(builder, loc, vec4Type, smemA,
                                             ValueRange{kInner, off});
            for (int64_t q = 0; q < 4; ++q)
              regA.push_back(vector::ExtractOp::create(builder, loc, v, q));
          }
        } else {
          for (int64_t i = 0; i < TM; ++i) {
            Value off = arith::AddIOp::create(builder, loc, tyTM, cIdx[i]);
            regA.push_back(memref::LoadOp::create(builder, loc, smemA,
                                                  ValueRange{kInner, off}));
          }
        }
        if (TN % 4 == 0) {
          for (int64_t j = 0; j < TN; j += 4) {
            Value off = arith::AddIOp::create(builder, loc, txTN, cIdx[j]);
            Value v = vector::LoadOp::create(builder, loc, vec4Type, smemB,
                                             ValueRange{kInner, off});
            for (int64_t q = 0; q < 4; ++q)
              regB.push_back(vector::ExtractOp::create(builder, loc, v, q));
          }
        } else {
          for (int64_t j = 0; j < TN; ++j) {
            Value off = arith::AddIOp::create(builder, loc, txTN, cIdx[j]);
            regB.push_back(memref::LoadOp::create(builder, loc, smemB,
                                                  ValueRange{kInner, off}));
          }
        }

        SmallVector<Value> nextAcc;
        nextAcc.reserve(TM * TN);
        for (int64_t i = 0; i < TM; ++i) {
          for (int64_t j = 0; j < TN; ++j) {
            Value sMul = arith::MulFOp::create(builder, loc, regA[i], regB[j]);
            nextAcc.push_back(arith::AddFOp::create(
                builder, loc, innerLoop.getRegionIterArg(i * TN + j), sMul));
          }
        }
        scf::YieldOp::create(builder, loc, nextAcc);

        builder.setInsertionPointAfter(innerLoop);

        gpu::BarrierOp::create(builder, loc);
        scf::YieldOp::create(builder, loc, innerLoop.getResults());

        builder.setInsertionPointAfter(loopK);

        // Store results (unrolled)
        for (int64_t i = 0; i < TM; ++i) {
          Value sRow =
              arith::AddIOp::create(builder, loc, threadRowStart, cIdx[i]);
          Value sRowCond = arith::CmpIOp::create(
              builder, loc, arith::CmpIPredicate::ult, sRow, M);
          for (int64_t j = 0; j < TN; ++j) {
            Value sCol =
                arith::AddIOp::create(builder, loc, threadColStart, cIdx[j]);
            Value sColCond = arith::CmpIOp::create(
                builder, loc, arith::CmpIPredicate::ult, sCol, N);
            Value sInBounds =
                arith::AndIOp::create(builder, loc, sRowCond, sColCond);
            auto storeCIf = scf::IfOp::create(builder, loc, sInBounds,
                                              /*withElseRegion=*/false);
            builder.setInsertionPointToStart(
                &storeCIf.getThenRegion().front());
            memref::StoreOp::create(
                builder, loc, applyEpilogue(loopK.getResult(i * TN + j), sCol),
                                    C, ValueRange{sRow, sCol});
            builder.setInsertionPointAfter(storeCIf);
          }
        }

        gpu::TerminatorOp::create(builder, loc);

        builder.setInsertionPointAfter(launchOp);
        finishDevice();
        gemmOp.erase();
      } else if (optLevel == 7) {
        // Level 7: SMEM double buffering on top of level 6. Shared memory
        // holds two copies of the A/B tiles; while the FMA loop consumes
        // buffer p, the next K-tile is prefetched into registers and then
        // committed to buffer 1-p. One barrier per K-tile instead of two.
        Value M = memref::DimOp::create(builder, loc, A, 0);
        Value K = memref::DimOp::create(builder, loc, A, 1);
        Value N = memref::DimOp::create(builder, loc, B, 1);

        Value zero = arith::ConstantIndexOp::create(builder, loc, 0);
        Value one = arith::ConstantIndexOp::create(builder, loc, 1);

        int64_t BM = 64;
        int64_t BN = 64;
        int64_t BK = 8;
        int64_t TM = 8;
        int64_t TN = 8;

        if (TileOverride ov = readTileOverride(); ov.valid) {
          BM = ov.BM; BN = ov.BN; BK = ov.BK; TM = ov.TM; TN = ov.TN;
        }

        if (auto dictAttr =
                gemmOp->getAttrOfType<DictionaryAttr>("tiling_params")) {
          if (auto attr =
                  llvm::dyn_cast_or_null<IntegerAttr>(dictAttr.get("tile_M")))
            BM = attr.getInt();
          if (auto attr =
                  llvm::dyn_cast_or_null<IntegerAttr>(dictAttr.get("tile_N")))
            BN = attr.getInt();
          if (auto attr =
                  llvm::dyn_cast_or_null<IntegerAttr>(dictAttr.get("tile_K")))
            BK = attr.getInt();
          if (auto attr = llvm::dyn_cast_or_null<IntegerAttr>(
                  dictAttr.get("thread_tile_M")))
            TM = attr.getInt();
          if (auto attr = llvm::dyn_cast_or_null<IntegerAttr>(
                  dictAttr.get("thread_tile_N")))
            TN = attr.getInt();
        }

        Value valBM = arith::ConstantIndexOp::create(builder, loc, BM);
        Value valBN = arith::ConstantIndexOp::create(builder, loc, BN);
        Value valBK = arith::ConstantIndexOp::create(builder, loc, BK);
        Value valTM = arith::ConstantIndexOp::create(builder, loc, TM);
        Value valTN = arith::ConstantIndexOp::create(builder, loc, TN);

        Value blockDimX = arith::ConstantIndexOp::create(builder, loc, BN / TN);
        Value blockDimY = arith::ConstantIndexOp::create(builder, loc, BM / TM);

        Value nMinus1 = arith::SubIOp::create(builder, loc, N, one);
        Value nAdd = arith::AddIOp::create(builder, loc, nMinus1, valBN);
        Value gridDimX = arith::DivUIOp::create(builder, loc, nAdd, valBN);

        Value mMinus1 = arith::SubIOp::create(builder, loc, M, one);
        Value mAdd = arith::AddIOp::create(builder, loc, mMinus1, valBM);
        Value gridDimY = arith::DivUIOp::create(builder, loc, mAdd, valBM);

        auto launchOp = gpu::LaunchOp::create(builder, loc, gridDimX, gridDimY,
                                              one, blockDimX, blockDimY, one);
        launchOp->setAttr(trscd::kGemmGeneratedMarker,
                          builder.getUnitAttr());

        builder.setInsertionPointToStart(&launchOp.getBody().front());

        auto smemSpace = gpu::AddressSpaceAttr::get(
            builder.getContext(), gpu::AddressSpace::Workgroup);
        auto f32Type = builder.getF32Type();
        auto vec4Type = VectorType::get({4}, f32Type);

        // Leading dim = double-buffer index. smemA stays transposed
        // (BK x BM) as in level 6.
        MemRefType smemAType = MemRefType::get(
            {2, BK, BM}, f32Type, MemRefLayoutAttrInterface(), smemSpace);
        MemRefType smemBType = MemRefType::get(
            {2, BK, BN}, f32Type, MemRefLayoutAttrInterface(), smemSpace);

        Value smemA = launchOp.addWorkgroupAttribution(smemAType, loc);
        Value smemB = launchOp.addWorkgroupAttribution(smemBType, loc);

        Value bx = launchOp.getBlockIds().x;
        Value by = launchOp.getBlockIds().y;
        Value tx = launchOp.getThreadIds().x;
        Value ty = launchOp.getThreadIds().y;

        Value threadColStart = arith::AddIOp::create(
            builder, loc, arith::MulIOp::create(builder, loc, bx, valBN),
            arith::MulIOp::create(builder, loc, tx, valTN));
        Value threadRowStart = arith::AddIOp::create(
            builder, loc, arith::MulIOp::create(builder, loc, by, valBM),
            arith::MulIOp::create(builder, loc, ty, valTM));

        Value tyTM = arith::MulIOp::create(builder, loc, ty, valTM);
        Value txTN = arith::MulIOp::create(builder, loc, tx, valTN);
        int64_t maxT = TM > TN ? TM : TN;
        if (maxT < 4)
          maxT = 4;
        SmallVector<Value> cIdx;
        for (int64_t t = 0; t < maxT; ++t)
          cIdx.push_back(arith::ConstantIndexOp::create(builder, loc, t));
        Value zeroF32 = arith::ConstantOp::create(builder, loc,
                                                  builder.getF32FloatAttr(0.0));

        SmallVector<Value> initAcc;
        initAcc.reserve(TM * TN);
        for (int64_t i = 0; i < TM; ++i) {
          Value rowInit =
              arith::AddIOp::create(builder, loc, threadRowStart, cIdx[i]);
          Value rowCondInit = arith::CmpIOp::create(
              builder, loc, arith::CmpIPredicate::ult, rowInit, M);
          for (int64_t j = 0; j < TN; ++j) {
            Value colInit =
                arith::AddIOp::create(builder, loc, threadColStart, cIdx[j]);
            Value colCondInit = arith::CmpIOp::create(
                builder, loc, arith::CmpIPredicate::ult, colInit, N);
            Value inBoundsInit =
                arith::AndIOp::create(builder, loc, rowCondInit, colCondInit);
            auto loadCIf = scf::IfOp::create(builder, loc, f32Type,
                                             inBoundsInit,
                                             /*withElseRegion=*/true);
            builder.setInsertionPointToStart(&loadCIf.getThenRegion().front());
            Value cVal = memref::LoadOp::create(builder, loc, C,
                                                ValueRange{rowInit, colInit});
            scf::YieldOp::create(builder, loc, cVal);
            builder.setInsertionPointToStart(&loadCIf.getElseRegion().front());
            scf::YieldOp::create(builder, loc, zeroF32);
            builder.setInsertionPointAfter(loadCIf);
            initAcc.push_back(loadCIf.getResult(0));
          }
        }

        Value val2 = arith::ConstantIndexOp::create(builder, loc, 2);
        Value val4 = arith::ConstantIndexOp::create(builder, loc, 4);

        Value tid = arith::AddIOp::create(
            builder, loc, arith::MulIOp::create(builder, loc, ty, blockDimX),
            tx);

        int64_t threadsPerBlock = (BM / TM) * (BN / TN);
        Value valThreads =
            arith::ConstantIndexOp::create(builder, loc, threadsPerBlock);

        Value valBKDiv4 = arith::ConstantIndexOp::create(builder, loc, BK / 4);
        Value bmTimesBkDiv4 =
            arith::ConstantIndexOp::create(builder, loc, (BM * BK) / 4);
        Value valBNDiv4 = arith::ConstantIndexOp::create(builder, loc, BN / 4);
        Value bkTimesBnDiv4 =
            arith::ConstantIndexOp::create(builder, loc, (BK * BN) / 4);

        // Cooperative load of the k=0 tile into shared buffer 0 (prologue
        // only; steady-state tiles arrive via the register-staged prefetch
        // below). Same float4 loads + transposed A scatter as level 6.
        auto emitTileLoads = [&](Value buf, Value bkVal) {
          auto loadALoop =
              scf::ForOp::create(builder, loc, tid, bmTimesBkDiv4, valThreads);
          builder.setInsertionPointToStart(loadALoop.getBody());
          Value linearA = loadALoop.getInductionVar();
          Value aRow = arith::DivUIOp::create(builder, loc, linearA, valBKDiv4);
          Value aColDiv4 =
              arith::RemUIOp::create(builder, loc, linearA, valBKDiv4);
          Value aCol = arith::MulIOp::create(builder, loc, aColDiv4, val4);

          Value globalARow = arith::AddIOp::create(
              builder, loc, arith::MulIOp::create(builder, loc, by, valBM),
              aRow);
          Value globalACol = arith::AddIOp::create(builder, loc, bkVal, aCol);

          Value aRowCond = arith::CmpIOp::create(
              builder, loc, arith::CmpIPredicate::ult, globalARow, M);
          Value aColCond = arith::CmpIOp::create(
              builder, loc, arith::CmpIPredicate::ult, globalACol, K);
          Value aCond = arith::AndIOp::create(builder, loc, aRowCond, aColCond);

          auto loadAIf =
              scf::IfOp::create(builder, loc, aCond, /*withElseRegion=*/false);
          builder.setInsertionPointToStart(&loadAIf.getThenRegion().front());

          Value aVecVal = vector::LoadOp::create(
              builder, loc, vec4Type, A, ValueRange{globalARow, globalACol});
          for (int64_t q = 0; q < 4; ++q) {
            Value elem = vector::ExtractOp::create(builder, loc, aVecVal, q);
            Value smRow = arith::AddIOp::create(builder, loc, aCol, cIdx[q]);
            memref::StoreOp::create(builder, loc, elem, smemA,
                                    ValueRange{buf, smRow, aRow});
          }

          builder.setInsertionPointAfter(loadAIf);
          builder.setInsertionPointAfter(loadALoop);

          auto loadBLoop =
              scf::ForOp::create(builder, loc, tid, bkTimesBnDiv4, valThreads);
          builder.setInsertionPointToStart(loadBLoop.getBody());
          Value linearB = loadBLoop.getInductionVar();
          Value bRow = arith::DivUIOp::create(builder, loc, linearB, valBNDiv4);
          Value bColDiv4 =
              arith::RemUIOp::create(builder, loc, linearB, valBNDiv4);
          Value bCol = arith::MulIOp::create(builder, loc, bColDiv4, val4);

          Value globalBRow = arith::AddIOp::create(builder, loc, bkVal, bRow);
          Value globalBCol = arith::AddIOp::create(
              builder, loc, arith::MulIOp::create(builder, loc, bx, valBN),
              bCol);

          Value bRowCond = arith::CmpIOp::create(
              builder, loc, arith::CmpIPredicate::ult, globalBRow, K);
          Value bColCond = arith::CmpIOp::create(
              builder, loc, arith::CmpIPredicate::ult, globalBCol, N);
          Value bCond = arith::AndIOp::create(builder, loc, bRowCond, bColCond);

          auto loadBIf =
              scf::IfOp::create(builder, loc, bCond, /*withElseRegion=*/false);
          builder.setInsertionPointToStart(&loadBIf.getThenRegion().front());

          Value bVecVal = vector::LoadOp::create(
              builder, loc, vec4Type, B, ValueRange{globalBRow, globalBCol});
          vector::StoreOp::create(builder, loc, bVecVal, smemB,
                                  ValueRange{buf, bRow, bCol});

          builder.setInsertionPointAfter(loadBIf);
          builder.setInsertionPointAfter(loadBLoop);
        };

        // Prologue: first K-tile into buffer 0.
        emitTileLoads(zero, zero);
        gpu::BarrierOp::create(builder, loc);

        auto loopK = scf::ForOp::create(builder, loc, zero, K, valBK, initAcc);
        if (!loopK.getBody()->empty()) {
          loopK.getBody()->getTerminator()->erase();
        }
        builder.setInsertionPointToStart(loopK.getBody());
        Value bk = loopK.getInductionVar();

        Value tileIdx = arith::DivUIOp::create(builder, loc, bk, valBK);
        Value bufIdx = arith::RemUIOp::create(builder, loc, tileIdx, val2);
        Value otherBuf = arith::SubIOp::create(builder, loc, one, bufIdx);

        // Register-staged prefetch: sm_75 has no cp.async, and a GMEM->SMEM
        // copy placed before the FMAs stalls the warp on the SMEM store
        // waiting for the global load. Instead issue the next tile's global
        // loads into registers here, run the FMA loop on the current buffer,
        // and commit the registers to the other buffer afterwards — the
        // global-load latency overlaps the FMAs.
        Value nextBk = arith::AddIOp::create(builder, loc, bk, valBK);
        Value hasNext = arith::CmpIOp::create(
            builder, loc, arith::CmpIPredicate::ult, nextBk, K);
        Value zeroVec = arith::ConstantOp::create(
            builder, loc, vec4Type, builder.getZeroAttr(vec4Type));

        int64_t vecsA = (BM * BK) / 4;
        int64_t vecsB = (BK * BN) / 4;
        int64_t slotsA = (vecsA + threadsPerBlock - 1) / threadsPerBlock;
        int64_t slotsB = (vecsB + threadsPerBlock - 1) / threadsPerBlock;

        SmallVector<Value> aPreVec, aPreCond, aPreCol, aPreRow;
        for (int64_t s = 0; s < slotsA; ++s) {
          Value linear =
              s == 0 ? tid
                     : arith::AddIOp::create(
                           builder, loc, tid,
                           arith::ConstantIndexOp::create(
                               builder, loc, s * threadsPerBlock));
          Value aRow = arith::DivUIOp::create(builder, loc, linear, valBKDiv4);
          Value aColDiv4 =
              arith::RemUIOp::create(builder, loc, linear, valBKDiv4);
          Value aCol = arith::MulIOp::create(builder, loc, aColDiv4, val4);
          Value globalARow = arith::AddIOp::create(
              builder, loc, arith::MulIOp::create(builder, loc, by, valBM),
              aRow);
          Value globalACol = arith::AddIOp::create(builder, loc, nextBk, aCol);
          Value cond = arith::AndIOp::create(
              builder, loc, hasNext,
              arith::AndIOp::create(
                  builder, loc,
                  arith::CmpIOp::create(builder, loc,
                                        arith::CmpIPredicate::ult, globalARow,
                                        M),
                  arith::CmpIOp::create(builder, loc,
                                        arith::CmpIPredicate::ult, globalACol,
                                        K)));
          if ((s + 1) * threadsPerBlock > vecsA)
            cond = arith::AndIOp::create(
                builder, loc, cond,
                arith::CmpIOp::create(
                    builder, loc, arith::CmpIPredicate::ult, linear,
                    arith::ConstantIndexOp::create(builder, loc, vecsA)));
          auto ldIf = scf::IfOp::create(builder, loc, vec4Type, cond,
                                        /*withElseRegion=*/true);
          builder.setInsertionPointToStart(&ldIf.getThenRegion().front());
          Value v = vector::LoadOp::create(builder, loc, vec4Type, A,
                                           ValueRange{globalARow, globalACol});
          scf::YieldOp::create(builder, loc, v);
          builder.setInsertionPointToStart(&ldIf.getElseRegion().front());
          scf::YieldOp::create(builder, loc, zeroVec);
          builder.setInsertionPointAfter(ldIf);
          aPreVec.push_back(ldIf.getResult(0));
          aPreCond.push_back(cond);
          aPreCol.push_back(aCol);
          aPreRow.push_back(aRow);
        }

        SmallVector<Value> bPreVec, bPreCond, bPreCol, bPreRow;
        for (int64_t s = 0; s < slotsB; ++s) {
          Value linear =
              s == 0 ? tid
                     : arith::AddIOp::create(
                           builder, loc, tid,
                           arith::ConstantIndexOp::create(
                               builder, loc, s * threadsPerBlock));
          Value bRow = arith::DivUIOp::create(builder, loc, linear, valBNDiv4);
          Value bColDiv4 =
              arith::RemUIOp::create(builder, loc, linear, valBNDiv4);
          Value bCol = arith::MulIOp::create(builder, loc, bColDiv4, val4);
          Value globalBRow = arith::AddIOp::create(builder, loc, nextBk, bRow);
          Value globalBCol = arith::AddIOp::create(
              builder, loc, arith::MulIOp::create(builder, loc, bx, valBN),
              bCol);
          Value cond = arith::AndIOp::create(
              builder, loc, hasNext,
              arith::AndIOp::create(
                  builder, loc,
                  arith::CmpIOp::create(builder, loc,
                                        arith::CmpIPredicate::ult, globalBRow,
                                        K),
                  arith::CmpIOp::create(builder, loc,
                                        arith::CmpIPredicate::ult, globalBCol,
                                        N)));
          if ((s + 1) * threadsPerBlock > vecsB)
            cond = arith::AndIOp::create(
                builder, loc, cond,
                arith::CmpIOp::create(
                    builder, loc, arith::CmpIPredicate::ult, linear,
                    arith::ConstantIndexOp::create(builder, loc, vecsB)));
          auto ldIf = scf::IfOp::create(builder, loc, vec4Type, cond,
                                        /*withElseRegion=*/true);
          builder.setInsertionPointToStart(&ldIf.getThenRegion().front());
          Value v = vector::LoadOp::create(builder, loc, vec4Type, B,
                                           ValueRange{globalBRow, globalBCol});
          scf::YieldOp::create(builder, loc, v);
          builder.setInsertionPointToStart(&ldIf.getElseRegion().front());
          scf::YieldOp::create(builder, loc, zeroVec);
          builder.setInsertionPointAfter(ldIf);
          bPreVec.push_back(ldIf.getResult(0));
          bPreCond.push_back(cond);
          bPreCol.push_back(bCol);
          bPreRow.push_back(bRow);
        }

        // Inner loop over BK; TMxTN FMAs unrolled on SSA values, reading the
        // current buffer.
        auto innerLoop = scf::ForOp::create(builder, loc, zero, valBK, one,
                                            loopK.getRegionIterArgs());
        if (!innerLoop.getBody()->empty()) {
          innerLoop.getBody()->getTerminator()->erase();
        }
        builder.setInsertionPointToStart(innerLoop.getBody());
        Value kInner = innerLoop.getInductionVar();

        SmallVector<Value> regA, regB;
        if (TM % 4 == 0) {
          for (int64_t i = 0; i < TM; i += 4) {
            Value off = arith::AddIOp::create(builder, loc, tyTM, cIdx[i]);
            Value v = vector::LoadOp::create(builder, loc, vec4Type, smemA,
                                             ValueRange{bufIdx, kInner, off});
            for (int64_t q = 0; q < 4; ++q)
              regA.push_back(vector::ExtractOp::create(builder, loc, v, q));
          }
        } else {
          for (int64_t i = 0; i < TM; ++i) {
            Value off = arith::AddIOp::create(builder, loc, tyTM, cIdx[i]);
            regA.push_back(memref::LoadOp::create(
                builder, loc, smemA, ValueRange{bufIdx, kInner, off}));
          }
        }
        if (TN % 4 == 0) {
          for (int64_t j = 0; j < TN; j += 4) {
            Value off = arith::AddIOp::create(builder, loc, txTN, cIdx[j]);
            Value v = vector::LoadOp::create(builder, loc, vec4Type, smemB,
                                             ValueRange{bufIdx, kInner, off});
            for (int64_t q = 0; q < 4; ++q)
              regB.push_back(vector::ExtractOp::create(builder, loc, v, q));
          }
        } else {
          for (int64_t j = 0; j < TN; ++j) {
            Value off = arith::AddIOp::create(builder, loc, txTN, cIdx[j]);
            regB.push_back(memref::LoadOp::create(
                builder, loc, smemB, ValueRange{bufIdx, kInner, off}));
          }
        }

        SmallVector<Value> nextAcc;
        nextAcc.reserve(TM * TN);
        for (int64_t i = 0; i < TM; ++i) {
          for (int64_t j = 0; j < TN; ++j) {
            Value sMul = arith::MulFOp::create(builder, loc, regA[i], regB[j]);
            nextAcc.push_back(arith::AddFOp::create(
                builder, loc, innerLoop.getRegionIterArg(i * TN + j), sMul));
          }
        }
        scf::YieldOp::create(builder, loc, nextAcc);

        builder.setInsertionPointAfter(innerLoop);

        // Commit the prefetched registers to the other buffer. Every other
        // thread's reads of that buffer finished before the previous
        // iteration's trailing barrier, so no barrier is needed first.
        for (int64_t s = 0; s < slotsA; ++s) {
          auto stIf = scf::IfOp::create(builder, loc, aPreCond[s],
                                        /*withElseRegion=*/false);
          builder.setInsertionPointToStart(&stIf.getThenRegion().front());
          for (int64_t q = 0; q < 4; ++q) {
            Value elem =
                vector::ExtractOp::create(builder, loc, aPreVec[s], q);
            Value smRow =
                arith::AddIOp::create(builder, loc, aPreCol[s], cIdx[q]);
            memref::StoreOp::create(builder, loc, elem, smemA,
                                    ValueRange{otherBuf, smRow, aPreRow[s]});
          }
          builder.setInsertionPointAfter(stIf);
        }
        for (int64_t s = 0; s < slotsB; ++s) {
          auto stIf = scf::IfOp::create(builder, loc, bPreCond[s],
                                        /*withElseRegion=*/false);
          builder.setInsertionPointToStart(&stIf.getThenRegion().front());
          vector::StoreOp::create(
              builder, loc, bPreVec[s], smemB,
              ValueRange{otherBuf, bPreRow[s], bPreCol[s]});
          builder.setInsertionPointAfter(stIf);
        }

        // Orders this iteration's reads of buffer p (and the commit writes
        // to buffer 1-p) before the next iteration begins.
        gpu::BarrierOp::create(builder, loc);
        scf::YieldOp::create(builder, loc, innerLoop.getResults());

        builder.setInsertionPointAfter(loopK);

        // Store results (unrolled)
        for (int64_t i = 0; i < TM; ++i) {
          Value sRow =
              arith::AddIOp::create(builder, loc, threadRowStart, cIdx[i]);
          Value sRowCond = arith::CmpIOp::create(
              builder, loc, arith::CmpIPredicate::ult, sRow, M);
          for (int64_t j = 0; j < TN; ++j) {
            Value sCol =
                arith::AddIOp::create(builder, loc, threadColStart, cIdx[j]);
            Value sColCond = arith::CmpIOp::create(
                builder, loc, arith::CmpIPredicate::ult, sCol, N);
            Value sInBounds =
                arith::AndIOp::create(builder, loc, sRowCond, sColCond);
            auto storeCIf = scf::IfOp::create(builder, loc, sInBounds,
                                              /*withElseRegion=*/false);
            builder.setInsertionPointToStart(
                &storeCIf.getThenRegion().front());
            memref::StoreOp::create(
                builder, loc, applyEpilogue(loopK.getResult(i * TN + j), sCol),
                C, ValueRange{sRow, sCol});
            builder.setInsertionPointAfter(storeCIf);
          }
        }

        gpu::TerminatorOp::create(builder, loc);

        builder.setInsertionPointAfter(launchOp);
        finishDevice();
        gemmOp.erase();
      } else if (optLevel == 8) {
        // Level 8: warp tiling on top of level 7's double buffering. The
        // block tile (BM x BN) splits into warp tiles (WM x WN); each warp
        // iterates WNITER subtiles of width WSUBN = WN/WNITER, and its 32
        // lanes cover a WSUBM x WSUBN patch with TM x TN thread tiles. All
        // SMEM reads within a warp then hit warp-coherent rows, and the
        // per-thread work stays at WMITER*TM x WNITER*TN accumulators.
        Value M = memref::DimOp::create(builder, loc, A, 0);
        Value K = memref::DimOp::create(builder, loc, A, 1);
        Value N = memref::DimOp::create(builder, loc, B, 1);

        Value zero = arith::ConstantIndexOp::create(builder, loc, 0);
        Value one = arith::ConstantIndexOp::create(builder, loc, 1);

        // Defaults autotuned on GTX 1650 (sm_75): best of a 16-config sweep
        // at N=512/1024/2048 (see bench/README.md). 128-thread blocks, 32
        // accumulators per thread, 16KB double-buffered SMEM.
        int64_t BM = 64;
        int64_t BN = 64;
        int64_t BK = 16;
        int64_t TM = 4;
        int64_t TN = 4;
        int64_t WM = 32;
        int64_t WN = 32;
        int64_t WNITER = 2;

        if (TileOverride ov = readTileOverride(); ov.valid) {
          BM = ov.BM; BN = ov.BN; BK = ov.BK; TM = ov.TM; TN = ov.TN;
          if (ov.hasWarp) {
            WM = ov.WM; WN = ov.WN; WNITER = ov.WNITER;
          }
        }

        if (auto dictAttr =
                gemmOp->getAttrOfType<DictionaryAttr>("tiling_params")) {
          if (auto attr =
                  llvm::dyn_cast_or_null<IntegerAttr>(dictAttr.get("tile_M")))
            BM = attr.getInt();
          if (auto attr =
                  llvm::dyn_cast_or_null<IntegerAttr>(dictAttr.get("tile_N")))
            BN = attr.getInt();
          if (auto attr =
                  llvm::dyn_cast_or_null<IntegerAttr>(dictAttr.get("tile_K")))
            BK = attr.getInt();
          if (auto attr = llvm::dyn_cast_or_null<IntegerAttr>(
                  dictAttr.get("thread_tile_M")))
            TM = attr.getInt();
          if (auto attr = llvm::dyn_cast_or_null<IntegerAttr>(
                  dictAttr.get("thread_tile_N")))
            TN = attr.getInt();
          if (auto attr = llvm::dyn_cast_or_null<IntegerAttr>(
                  dictAttr.get("warp_tile_M")))
            WM = attr.getInt();
          if (auto attr = llvm::dyn_cast_or_null<IntegerAttr>(
                  dictAttr.get("warp_tile_N")))
            WN = attr.getInt();
          if (auto attr = llvm::dyn_cast_or_null<IntegerAttr>(
                  dictAttr.get("warp_n_iter")))
            WNITER = attr.getInt();
        }

        // Derive the warp-internal layout; fall back to the defaults if the
        // requested shape doesn't decompose cleanly into 32-lane warps.
        auto deriveOk = [&]() {
          if (BK % 4 || BN % 4 || BM % WM || BN % WN || WN % WNITER)
            return false;
          int64_t wsubN = WN / WNITER;
          if (wsubN % TN)
            return false;
          int64_t cols = wsubN / TN;
          if (cols <= 0 || 32 % cols)
            return false;
          int64_t rows = 32 / cols;
          int64_t wsubM = rows * TM;
          if (wsubM <= 0 || WM % wsubM)
            return false;
          return true;
        };
        if (!deriveOk()) {
          BM = 64; BN = 64; BK = 16; TM = 4; TN = 4; WM = 32; WN = 32;
          WNITER = 2;
        }
        int64_t WSUBN = WN / WNITER;
        int64_t laneCols = WSUBN / TN;
        int64_t laneRows = 32 / laneCols;
        int64_t WSUBM = laneRows * TM;
        int64_t WMITER = WM / WSUBM;
        int64_t numWarps = (BM / WM) * (BN / WN);
        int64_t threadsPerBlock = numWarps * 32;
        int64_t regRows = WMITER * TM;
        int64_t regCols = WNITER * TN;

        Value valBM = arith::ConstantIndexOp::create(builder, loc, BM);
        Value valBN = arith::ConstantIndexOp::create(builder, loc, BN);
        Value valBK = arith::ConstantIndexOp::create(builder, loc, BK);

        Value valThreads =
            arith::ConstantIndexOp::create(builder, loc, threadsPerBlock);

        Value nMinus1 = arith::SubIOp::create(builder, loc, N, one);
        Value nAdd = arith::AddIOp::create(builder, loc, nMinus1, valBN);
        Value gridDimX = arith::DivUIOp::create(builder, loc, nAdd, valBN);

        Value mMinus1 = arith::SubIOp::create(builder, loc, M, one);
        Value mAdd = arith::AddIOp::create(builder, loc, mMinus1, valBM);
        Value gridDimY = arith::DivUIOp::create(builder, loc, mAdd, valBM);

        auto launchOp = gpu::LaunchOp::create(builder, loc, gridDimX, gridDimY,
                                              one, valThreads, one, one);
        launchOp->setAttr(trscd::kGemmGeneratedMarker,
                          builder.getUnitAttr());

        builder.setInsertionPointToStart(&launchOp.getBody().front());

        auto smemSpace = gpu::AddressSpaceAttr::get(
            builder.getContext(), gpu::AddressSpace::Workgroup);
        auto f32Type = builder.getF32Type();
        auto vec4Type = VectorType::get({4}, f32Type);

        MemRefType smemAType = MemRefType::get(
            {2, BK, BM}, f32Type, MemRefLayoutAttrInterface(), smemSpace);
        MemRefType smemBType = MemRefType::get(
            {2, BK, BN}, f32Type, MemRefLayoutAttrInterface(), smemSpace);

        Value smemA = launchOp.addWorkgroupAttribution(smemAType, loc);
        Value smemB = launchOp.addWorkgroupAttribution(smemBType, loc);

        Value bx = launchOp.getBlockIds().x;
        Value by = launchOp.getBlockIds().y;
        Value tid = launchOp.getThreadIds().x;

        auto cst = [&](int64_t v) {
          return arith::ConstantIndexOp::create(builder, loc, v);
        };

        // Warp / lane decomposition of the flat thread id.
        Value val32 = cst(32);
        Value warpId = arith::DivUIOp::create(builder, loc, tid, val32);
        Value laneId = arith::RemUIOp::create(builder, loc, tid, val32);
        Value warpsPerRow = cst(BN / WN);
        Value warpRow = arith::DivUIOp::create(builder, loc, warpId,
                                               warpsPerRow);
        Value warpCol = arith::RemUIOp::create(builder, loc, warpId,
                                               warpsPerRow);
        Value valLaneCols = cst(laneCols);
        Value laneRow = arith::DivUIOp::create(builder, loc, laneId,
                                               valLaneCols);
        Value laneCol = arith::RemUIOp::create(builder, loc, laneId,
                                               valLaneCols);

        // Per-thread row/col starts inside the block tile, one per warp
        // subtile: rowInBlock(wm) = warpRow*WM + wm*WSUBM + laneRow*TM,
        // colInBlock(wn) = warpCol*WN + wn*WSUBN + laneCol*TN.
        Value blockRow = arith::MulIOp::create(builder, loc, by, valBM);
        Value blockCol = arith::MulIOp::create(builder, loc, bx, valBN);
        Value warpRowOff =
            arith::MulIOp::create(builder, loc, warpRow, cst(WM));
        Value warpColOff =
            arith::MulIOp::create(builder, loc, warpCol, cst(WN));
        Value laneRowOff =
            arith::MulIOp::create(builder, loc, laneRow, cst(TM));
        Value laneColOff =
            arith::MulIOp::create(builder, loc, laneCol, cst(TN));

        SmallVector<Value> rowInBlock(WMITER), colInBlock(WNITER);
        SmallVector<Value> rowStart(WMITER), colStart(WNITER);
        for (int64_t wm = 0; wm < WMITER; ++wm) {
          Value r = arith::AddIOp::create(builder, loc, warpRowOff,
                                          laneRowOff);
          if (wm > 0)
            r = arith::AddIOp::create(builder, loc, r, cst(wm * WSUBM));
          rowInBlock[wm] = r;
          rowStart[wm] = arith::AddIOp::create(builder, loc, blockRow, r);
        }
        for (int64_t wn = 0; wn < WNITER; ++wn) {
          Value c = arith::AddIOp::create(builder, loc, warpColOff,
                                          laneColOff);
          if (wn > 0)
            c = arith::AddIOp::create(builder, loc, c, cst(wn * WSUBN));
          colInBlock[wn] = c;
          colStart[wn] = arith::AddIOp::create(builder, loc, blockCol, c);
        }

        int64_t maxT = TM > TN ? TM : TN;
        if (maxT < 4)
          maxT = 4;
        SmallVector<Value> cIdx;
        for (int64_t t = 0; t < maxT; ++t)
          cIdx.push_back(cst(t));
        Value zeroF32 = arith::ConstantOp::create(builder, loc,
                                                  builder.getF32FloatAttr(0.0));

        // Accumulators from C; flat index (wm*TM+i)*regCols + wn*TN+j.
        SmallVector<Value> initAcc;
        initAcc.reserve(regRows * regCols);
        for (int64_t wm = 0; wm < WMITER; ++wm) {
          for (int64_t i = 0; i < TM; ++i) {
            Value rowInit =
                arith::AddIOp::create(builder, loc, rowStart[wm], cIdx[i]);
            Value rowCondInit = arith::CmpIOp::create(
                builder, loc, arith::CmpIPredicate::ult, rowInit, M);
            for (int64_t wn = 0; wn < WNITER; ++wn) {
              for (int64_t j = 0; j < TN; ++j) {
                Value colInit = arith::AddIOp::create(builder, loc,
                                                      colStart[wn], cIdx[j]);
                Value colCondInit = arith::CmpIOp::create(
                    builder, loc, arith::CmpIPredicate::ult, colInit, N);
                Value inBoundsInit = arith::AndIOp::create(
                    builder, loc, rowCondInit, colCondInit);
                auto loadCIf = scf::IfOp::create(builder, loc, f32Type,
                                                 inBoundsInit,
                                                 /*withElseRegion=*/true);
                builder.setInsertionPointToStart(
                    &loadCIf.getThenRegion().front());
                Value cVal = memref::LoadOp::create(
                    builder, loc, C, ValueRange{rowInit, colInit});
                scf::YieldOp::create(builder, loc, cVal);
                builder.setInsertionPointToStart(
                    &loadCIf.getElseRegion().front());
                scf::YieldOp::create(builder, loc, zeroF32);
                builder.setInsertionPointAfter(loadCIf);
                initAcc.push_back(loadCIf.getResult(0));
              }
            }
          }
        }

        Value val2 = cst(2);
        Value val4 = cst(4);
        Value valBKDiv4 = cst(BK / 4);
        Value valBNDiv4 = cst(BN / 4);
        Value bmTimesBkDiv4 = cst((BM * BK) / 4);
        Value bkTimesBnDiv4 = cst((BK * BN) / 4);

        // Cooperative prologue load of the k=0 tile into buffer 0 (same
        // float4 loads + transposed A scatter as levels 6/7).
        auto emitTileLoads = [&](Value buf, Value bkVal) {
          auto loadALoop =
              scf::ForOp::create(builder, loc, tid, bmTimesBkDiv4, valThreads);
          builder.setInsertionPointToStart(loadALoop.getBody());
          Value linearA = loadALoop.getInductionVar();
          Value aRow = arith::DivUIOp::create(builder, loc, linearA, valBKDiv4);
          Value aColDiv4 =
              arith::RemUIOp::create(builder, loc, linearA, valBKDiv4);
          Value aCol = arith::MulIOp::create(builder, loc, aColDiv4, val4);

          Value globalARow =
              arith::AddIOp::create(builder, loc, blockRow, aRow);
          Value globalACol = arith::AddIOp::create(builder, loc, bkVal, aCol);

          Value aRowCond = arith::CmpIOp::create(
              builder, loc, arith::CmpIPredicate::ult, globalARow, M);
          Value aColCond = arith::CmpIOp::create(
              builder, loc, arith::CmpIPredicate::ult, globalACol, K);
          Value aCond = arith::AndIOp::create(builder, loc, aRowCond, aColCond);

          auto loadAIf =
              scf::IfOp::create(builder, loc, aCond, /*withElseRegion=*/false);
          builder.setInsertionPointToStart(&loadAIf.getThenRegion().front());

          Value aVecVal = vector::LoadOp::create(
              builder, loc, vec4Type, A, ValueRange{globalARow, globalACol});
          for (int64_t q = 0; q < 4; ++q) {
            Value elem = vector::ExtractOp::create(builder, loc, aVecVal, q);
            Value smRow = arith::AddIOp::create(builder, loc, aCol, cIdx[q]);
            memref::StoreOp::create(builder, loc, elem, smemA,
                                    ValueRange{buf, smRow, aRow});
          }

          builder.setInsertionPointAfter(loadAIf);
          builder.setInsertionPointAfter(loadALoop);

          auto loadBLoop =
              scf::ForOp::create(builder, loc, tid, bkTimesBnDiv4, valThreads);
          builder.setInsertionPointToStart(loadBLoop.getBody());
          Value linearB = loadBLoop.getInductionVar();
          Value bRow = arith::DivUIOp::create(builder, loc, linearB, valBNDiv4);
          Value bColDiv4 =
              arith::RemUIOp::create(builder, loc, linearB, valBNDiv4);
          Value bCol = arith::MulIOp::create(builder, loc, bColDiv4, val4);

          Value globalBRow = arith::AddIOp::create(builder, loc, bkVal, bRow);
          Value globalBCol =
              arith::AddIOp::create(builder, loc, blockCol, bCol);

          Value bRowCond = arith::CmpIOp::create(
              builder, loc, arith::CmpIPredicate::ult, globalBRow, K);
          Value bColCond = arith::CmpIOp::create(
              builder, loc, arith::CmpIPredicate::ult, globalBCol, N);
          Value bCond = arith::AndIOp::create(builder, loc, bRowCond, bColCond);

          auto loadBIf =
              scf::IfOp::create(builder, loc, bCond, /*withElseRegion=*/false);
          builder.setInsertionPointToStart(&loadBIf.getThenRegion().front());

          Value bVecVal = vector::LoadOp::create(
              builder, loc, vec4Type, B, ValueRange{globalBRow, globalBCol});
          vector::StoreOp::create(builder, loc, bVecVal, smemB,
                                  ValueRange{buf, bRow, bCol});

          builder.setInsertionPointAfter(loadBIf);
          builder.setInsertionPointAfter(loadBLoop);
        };

        emitTileLoads(zero, zero);
        gpu::BarrierOp::create(builder, loc);

        auto loopK = scf::ForOp::create(builder, loc, zero, K, valBK, initAcc);
        if (!loopK.getBody()->empty()) {
          loopK.getBody()->getTerminator()->erase();
        }
        builder.setInsertionPointToStart(loopK.getBody());
        Value bk = loopK.getInductionVar();

        Value tileIdx = arith::DivUIOp::create(builder, loc, bk, valBK);
        Value bufIdx = arith::RemUIOp::create(builder, loc, tileIdx, val2);
        Value otherBuf = arith::SubIOp::create(builder, loc, one, bufIdx);

        // Register-staged prefetch of the next K-tile (see level 7).
        Value nextBk = arith::AddIOp::create(builder, loc, bk, valBK);
        Value hasNext = arith::CmpIOp::create(
            builder, loc, arith::CmpIPredicate::ult, nextBk, K);
        Value zeroVec = arith::ConstantOp::create(
            builder, loc, vec4Type, builder.getZeroAttr(vec4Type));

        int64_t vecsA = (BM * BK) / 4;
        int64_t vecsB = (BK * BN) / 4;
        int64_t slotsA = (vecsA + threadsPerBlock - 1) / threadsPerBlock;
        int64_t slotsB = (vecsB + threadsPerBlock - 1) / threadsPerBlock;

        SmallVector<Value> aPreVec, aPreCond, aPreCol, aPreRow;
        for (int64_t s = 0; s < slotsA; ++s) {
          Value linear =
              s == 0 ? tid
                     : arith::AddIOp::create(builder, loc, tid,
                                             cst(s * threadsPerBlock));
          Value aRow = arith::DivUIOp::create(builder, loc, linear, valBKDiv4);
          Value aColDiv4 =
              arith::RemUIOp::create(builder, loc, linear, valBKDiv4);
          Value aCol = arith::MulIOp::create(builder, loc, aColDiv4, val4);
          Value globalARow =
              arith::AddIOp::create(builder, loc, blockRow, aRow);
          Value globalACol = arith::AddIOp::create(builder, loc, nextBk, aCol);
          Value cond = arith::AndIOp::create(
              builder, loc, hasNext,
              arith::AndIOp::create(
                  builder, loc,
                  arith::CmpIOp::create(builder, loc,
                                        arith::CmpIPredicate::ult, globalARow,
                                        M),
                  arith::CmpIOp::create(builder, loc,
                                        arith::CmpIPredicate::ult, globalACol,
                                        K)));
          if ((s + 1) * threadsPerBlock > vecsA)
            cond = arith::AndIOp::create(
                builder, loc, cond,
                arith::CmpIOp::create(builder, loc,
                                      arith::CmpIPredicate::ult, linear,
                                      cst(vecsA)));
          auto ldIf = scf::IfOp::create(builder, loc, vec4Type, cond,
                                        /*withElseRegion=*/true);
          builder.setInsertionPointToStart(&ldIf.getThenRegion().front());
          Value v = vector::LoadOp::create(builder, loc, vec4Type, A,
                                           ValueRange{globalARow, globalACol});
          scf::YieldOp::create(builder, loc, v);
          builder.setInsertionPointToStart(&ldIf.getElseRegion().front());
          scf::YieldOp::create(builder, loc, zeroVec);
          builder.setInsertionPointAfter(ldIf);
          aPreVec.push_back(ldIf.getResult(0));
          aPreCond.push_back(cond);
          aPreCol.push_back(aCol);
          aPreRow.push_back(aRow);
        }

        SmallVector<Value> bPreVec, bPreCond, bPreCol, bPreRow;
        for (int64_t s = 0; s < slotsB; ++s) {
          Value linear =
              s == 0 ? tid
                     : arith::AddIOp::create(builder, loc, tid,
                                             cst(s * threadsPerBlock));
          Value bRow = arith::DivUIOp::create(builder, loc, linear, valBNDiv4);
          Value bColDiv4 =
              arith::RemUIOp::create(builder, loc, linear, valBNDiv4);
          Value bCol = arith::MulIOp::create(builder, loc, bColDiv4, val4);
          Value globalBRow = arith::AddIOp::create(builder, loc, nextBk, bRow);
          Value globalBCol =
              arith::AddIOp::create(builder, loc, blockCol, bCol);
          Value cond = arith::AndIOp::create(
              builder, loc, hasNext,
              arith::AndIOp::create(
                  builder, loc,
                  arith::CmpIOp::create(builder, loc,
                                        arith::CmpIPredicate::ult, globalBRow,
                                        K),
                  arith::CmpIOp::create(builder, loc,
                                        arith::CmpIPredicate::ult, globalBCol,
                                        N)));
          if ((s + 1) * threadsPerBlock > vecsB)
            cond = arith::AndIOp::create(
                builder, loc, cond,
                arith::CmpIOp::create(builder, loc,
                                      arith::CmpIPredicate::ult, linear,
                                      cst(vecsB)));
          auto ldIf = scf::IfOp::create(builder, loc, vec4Type, cond,
                                        /*withElseRegion=*/true);
          builder.setInsertionPointToStart(&ldIf.getThenRegion().front());
          Value v = vector::LoadOp::create(builder, loc, vec4Type, B,
                                           ValueRange{globalBRow, globalBCol});
          scf::YieldOp::create(builder, loc, v);
          builder.setInsertionPointToStart(&ldIf.getElseRegion().front());
          scf::YieldOp::create(builder, loc, zeroVec);
          builder.setInsertionPointAfter(ldIf);
          bPreVec.push_back(ldIf.getResult(0));
          bPreCond.push_back(cond);
          bPreCol.push_back(bCol);
          bPreRow.push_back(bRow);
        }

        // Inner loop over BK; per k: load warp fragments, then
        // WMITER x TM x WNITER x TN FMAs on SSA accumulators.
        auto innerLoop = scf::ForOp::create(builder, loc, zero, valBK, one,
                                            loopK.getRegionIterArgs());
        if (!innerLoop.getBody()->empty()) {
          innerLoop.getBody()->getTerminator()->erase();
        }
        builder.setInsertionPointToStart(innerLoop.getBody());
        Value kInner = innerLoop.getInductionVar();

        SmallVector<Value> regA(regRows), regB(regCols);
        for (int64_t wm = 0; wm < WMITER; ++wm) {
          if (TM % 4 == 0) {
            for (int64_t i = 0; i < TM; i += 4) {
              Value off = arith::AddIOp::create(builder, loc, rowInBlock[wm],
                                                cIdx[i]);
              Value v = vector::LoadOp::create(
                  builder, loc, vec4Type, smemA,
                  ValueRange{bufIdx, kInner, off});
              for (int64_t q = 0; q < 4; ++q)
                regA[wm * TM + i + q] =
                    vector::ExtractOp::create(builder, loc, v, q);
            }
          } else {
            for (int64_t i = 0; i < TM; ++i) {
              Value off = arith::AddIOp::create(builder, loc, rowInBlock[wm],
                                                cIdx[i]);
              regA[wm * TM + i] = memref::LoadOp::create(
                  builder, loc, smemA, ValueRange{bufIdx, kInner, off});
            }
          }
        }
        for (int64_t wn = 0; wn < WNITER; ++wn) {
          if (TN % 4 == 0) {
            for (int64_t j = 0; j < TN; j += 4) {
              Value off = arith::AddIOp::create(builder, loc, colInBlock[wn],
                                                cIdx[j]);
              Value v = vector::LoadOp::create(
                  builder, loc, vec4Type, smemB,
                  ValueRange{bufIdx, kInner, off});
              for (int64_t q = 0; q < 4; ++q)
                regB[wn * TN + j + q] =
                    vector::ExtractOp::create(builder, loc, v, q);
            }
          } else {
            for (int64_t j = 0; j < TN; ++j) {
              Value off = arith::AddIOp::create(builder, loc, colInBlock[wn],
                                                cIdx[j]);
              regB[wn * TN + j] = memref::LoadOp::create(
                  builder, loc, smemB, ValueRange{bufIdx, kInner, off});
            }
          }
        }

        SmallVector<Value> nextAcc;
        nextAcc.reserve(regRows * regCols);
        for (int64_t r = 0; r < regRows; ++r) {
          for (int64_t c = 0; c < regCols; ++c) {
            Value sMul = arith::MulFOp::create(builder, loc, regA[r], regB[c]);
            nextAcc.push_back(arith::AddFOp::create(
                builder, loc, innerLoop.getRegionIterArg(r * regCols + c),
                sMul));
          }
        }
        scf::YieldOp::create(builder, loc, nextAcc);

        builder.setInsertionPointAfter(innerLoop);

        // Commit the prefetched registers to the other buffer, then the one
        // barrier per K-tile (see level 7 for the ordering argument).
        for (int64_t s = 0; s < slotsA; ++s) {
          auto stIf = scf::IfOp::create(builder, loc, aPreCond[s],
                                        /*withElseRegion=*/false);
          builder.setInsertionPointToStart(&stIf.getThenRegion().front());
          for (int64_t q = 0; q < 4; ++q) {
            Value elem =
                vector::ExtractOp::create(builder, loc, aPreVec[s], q);
            Value smRow =
                arith::AddIOp::create(builder, loc, aPreCol[s], cIdx[q]);
            memref::StoreOp::create(builder, loc, elem, smemA,
                                    ValueRange{otherBuf, smRow, aPreRow[s]});
          }
          builder.setInsertionPointAfter(stIf);
        }
        for (int64_t s = 0; s < slotsB; ++s) {
          auto stIf = scf::IfOp::create(builder, loc, bPreCond[s],
                                        /*withElseRegion=*/false);
          builder.setInsertionPointToStart(&stIf.getThenRegion().front());
          vector::StoreOp::create(
              builder, loc, bPreVec[s], smemB,
              ValueRange{otherBuf, bPreRow[s], bPreCol[s]});
          builder.setInsertionPointAfter(stIf);
        }

        gpu::BarrierOp::create(builder, loc);
        scf::YieldOp::create(builder, loc, innerLoop.getResults());

        builder.setInsertionPointAfter(loopK);

        // Store results (unrolled, epilogue applied)
        for (int64_t wm = 0; wm < WMITER; ++wm) {
          for (int64_t i = 0; i < TM; ++i) {
            Value sRow =
                arith::AddIOp::create(builder, loc, rowStart[wm], cIdx[i]);
            Value sRowCond = arith::CmpIOp::create(
                builder, loc, arith::CmpIPredicate::ult, sRow, M);
            for (int64_t wn = 0; wn < WNITER; ++wn) {
              for (int64_t j = 0; j < TN; ++j) {
                Value sCol = arith::AddIOp::create(builder, loc,
                                                   colStart[wn], cIdx[j]);
                Value sColCond = arith::CmpIOp::create(
                    builder, loc, arith::CmpIPredicate::ult, sCol, N);
                Value sInBounds = arith::AndIOp::create(builder, loc,
                                                        sRowCond, sColCond);
                auto storeCIf = scf::IfOp::create(builder, loc, sInBounds,
                                                  /*withElseRegion=*/false);
                builder.setInsertionPointToStart(
                    &storeCIf.getThenRegion().front());
                int64_t accIdx =
                    (wm * TM + i) * regCols + wn * TN + j;
                memref::StoreOp::create(
                    builder, loc,
                    applyEpilogue(loopK.getResult(accIdx), sCol), C,
                    ValueRange{sRow, sCol});
                builder.setInsertionPointAfter(storeCIf);
              }
            }
          }
        }

        gpu::TerminatorOp::create(builder, loc);

        builder.setInsertionPointAfter(launchOp);
        finishDevice();
        gemmOp.erase();
      }
    });
  }
};
} // namespace

namespace mlir {
namespace trscd {

std::unique_ptr<mlir::Pass> createLowerTrscdMatMulPass(int optLevel) {
  return std::make_unique<LowerTrscdMatMulPass>(optLevel);
}

} // namespace trscd
} // namespace mlir
