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

using namespace mlir;

namespace {
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

      // GPU levels launch kernels on host-allocated memrefs; pin them so the
      // device can access them (lowers to mgpuMemHostRegisterMemRef).
      if (optLevel >= 2) {
        auto registerHost = [&](Value memref) {
          auto type = cast<MemRefType>(memref.getType());
          auto unrankedType = UnrankedMemRefType::get(type.getElementType(),
                                                      type.getMemorySpace());
          Value casted =
              memref::CastOp::create(builder, loc, unrankedType, memref);
          gpu::HostRegisterOp::create(builder, loc, casted);
        };
        registerHost(A);
        registerHost(B);
        registerHost(C);
      }

      if (optLevel == 1) {
        Value M = memref::DimOp::create(builder, loc, A, 0);
        Value K = memref::DimOp::create(builder, loc, A, 1);
        Value N = memref::DimOp::create(builder, loc, B, 1);

        Value zero = arith::ConstantIndexOp::create(builder, loc, 0);
        Value one = arith::ConstantIndexOp::create(builder, loc, 1);

        auto loopI = scf::ForOp::create(builder, loc, zero, M, one);
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
            builder, loc, loopK.getResult(0), C,
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

        memref::StoreOp::create(builder, loc, add, C, ValueRange{row, col});

        builder.setInsertionPointAfter(ifOp);
        gpu::TerminatorOp::create(builder, loc);

        builder.setInsertionPointAfter(launchOp);
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
        memref::StoreOp::create(builder, loc, loopK.getResult(0), C,
                                ValueRange{row, col});

        builder.setInsertionPointAfter(storeCIf);
        gpu::TerminatorOp::create(builder, loc);

        builder.setInsertionPointAfter(launchOp);
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

        // Thread accumulators
        MemRefType regType = MemRefType::get({TM}, f32Type);
        Value threadAcc = memref::AllocaOp::create(builder, loc, regType);

        // Initialize accumulators from C
        auto initLoop = scf::ForOp::create(builder, loc, zero, valTM, one);
        builder.setInsertionPointToStart(initLoop.getBody());
        Value iInit = initLoop.getInductionVar();
        Value rowInit =
            arith::AddIOp::create(builder, loc, threadRowStart, iInit);

        Value rowCondInit = arith::CmpIOp::create(
            builder, loc, arith::CmpIPredicate::ult, rowInit, M);
        Value colCondInit = arith::CmpIOp::create(
            builder, loc, arith::CmpIPredicate::ult, col, N);
        Value inBoundsInit =
            arith::AndIOp::create(builder, loc, rowCondInit, colCondInit);

        auto loadCIf = scf::IfOp::create(builder, loc, f32Type, inBoundsInit,
                                         /*withElseRegion=*/true);
        builder.setInsertionPointToStart(&loadCIf.getThenRegion().front());
        Value cVal =
            memref::LoadOp::create(builder, loc, C, ValueRange{rowInit, col});
        scf::YieldOp::create(builder, loc, cVal);
        builder.setInsertionPointToStart(&loadCIf.getElseRegion().front());
        Value zeroF32 = arith::ConstantOp::create(builder, loc,
                                                  builder.getF32FloatAttr(0.0));
        scf::YieldOp::create(builder, loc, zeroF32);

        builder.setInsertionPointAfter(loadCIf);
        memref::StoreOp::create(builder, loc, loadCIf.getResult(0), threadAcc,
                                ValueRange{iInit});

        builder.setInsertionPointAfter(initLoop);

        // Outer K loop
        auto loopK = scf::ForOp::create(builder, loc, zero, K, valBK);
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

        // Inner loop over K
        auto innerLoop = scf::ForOp::create(builder, loc, zero, valBK, one);
        builder.setInsertionPointToStart(innerLoop.getBody());
        Value kInner = innerLoop.getInductionVar();

        Value sB =
            memref::LoadOp::create(builder, loc, smemB, ValueRange{kInner, tx});

        // Loop over TM
        auto tmLoop = scf::ForOp::create(builder, loc, zero, valTM, one);
        builder.setInsertionPointToStart(tmLoop.getBody());
        Value resIdx = tmLoop.getInductionVar();
        Value aSmRow = arith::AddIOp::create(
            builder, loc, arith::MulIOp::create(builder, loc, ty, valTM),
            resIdx);

        Value sA = memref::LoadOp::create(builder, loc, smemA,
                                          ValueRange{aSmRow, kInner});
        Value sMul = arith::MulFOp::create(builder, loc, sA, sB);
        Value curAcc =
            memref::LoadOp::create(builder, loc, threadAcc, ValueRange{resIdx});
        Value nextAcc = arith::AddFOp::create(builder, loc, curAcc, sMul);
        memref::StoreOp::create(builder, loc, nextAcc, threadAcc,
                                ValueRange{resIdx});

        builder.setInsertionPointAfter(tmLoop);
        builder.setInsertionPointAfter(innerLoop);

        gpu::BarrierOp::create(builder, loc);

        builder.setInsertionPointAfter(loopK);

        // Store results
        auto storeLoop = scf::ForOp::create(builder, loc, zero, valTM, one);
        builder.setInsertionPointToStart(storeLoop.getBody());
        Value sI = storeLoop.getInductionVar();
        Value sRow = arith::AddIOp::create(builder, loc, threadRowStart, sI);

        Value sRowCond = arith::CmpIOp::create(
            builder, loc, arith::CmpIPredicate::ult, sRow, M);
        Value sColCond = arith::CmpIOp::create(
            builder, loc, arith::CmpIPredicate::ult, col, N);
        Value sInBounds =
            arith::AndIOp::create(builder, loc, sRowCond, sColCond);

        auto storeCIf = scf::IfOp::create(builder, loc, sInBounds,
                                          /*withElseRegion=*/false);
        builder.setInsertionPointToStart(&storeCIf.getThenRegion().front());
        Value finalAcc =
            memref::LoadOp::create(builder, loc, threadAcc, ValueRange{sI});
        memref::StoreOp::create(builder, loc, finalAcc, C,
                                ValueRange{sRow, col});

        builder.setInsertionPointAfter(storeCIf);
        builder.setInsertionPointAfter(storeLoop);

        gpu::TerminatorOp::create(builder, loc);

        builder.setInsertionPointAfter(launchOp);
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

        // Thread accumulators
        MemRefType regType = MemRefType::get({TM, TN}, f32Type);
        Value threadAcc = memref::AllocaOp::create(builder, loc, regType);

        MemRefType regAType = MemRefType::get({TM}, f32Type);
        MemRefType regBType = MemRefType::get({TN}, f32Type);
        Value regA = memref::AllocaOp::create(builder, loc, regAType);
        Value regB = memref::AllocaOp::create(builder, loc, regBType);

        // Initialize accumulators from C
        auto initLoopI = scf::ForOp::create(builder, loc, zero, valTM, one);
        builder.setInsertionPointToStart(initLoopI.getBody());
        Value iInit = initLoopI.getInductionVar();
        Value rowInit =
            arith::AddIOp::create(builder, loc, threadRowStart, iInit);
        Value rowCondInit = arith::CmpIOp::create(
            builder, loc, arith::CmpIPredicate::ult, rowInit, M);

        auto initLoopJ = scf::ForOp::create(builder, loc, zero, valTN, one);
        builder.setInsertionPointToStart(initLoopJ.getBody());
        Value jInit = initLoopJ.getInductionVar();
        Value colInit =
            arith::AddIOp::create(builder, loc, threadColStart, jInit);

        Value colCondInit = arith::CmpIOp::create(
            builder, loc, arith::CmpIPredicate::ult, colInit, N);
        Value inBoundsInit =
            arith::AndIOp::create(builder, loc, rowCondInit, colCondInit);

        auto loadCIf = scf::IfOp::create(builder, loc, f32Type, inBoundsInit,
                                         /*withElseRegion=*/true);
        builder.setInsertionPointToStart(&loadCIf.getThenRegion().front());
        Value cVal = memref::LoadOp::create(builder, loc, C,
                                            ValueRange{rowInit, colInit});
        scf::YieldOp::create(builder, loc, cVal);
        builder.setInsertionPointToStart(&loadCIf.getElseRegion().front());
        Value zeroF32 = arith::ConstantOp::create(builder, loc,
                                                  builder.getF32FloatAttr(0.0));
        scf::YieldOp::create(builder, loc, zeroF32);

        builder.setInsertionPointAfter(loadCIf);
        memref::StoreOp::create(builder, loc, loadCIf.getResult(0), threadAcc,
                                ValueRange{iInit, jInit});

        builder.setInsertionPointAfter(initLoopJ);
        builder.setInsertionPointAfter(initLoopI);

        // Outer K loop
        auto loopK = scf::ForOp::create(builder, loc, zero, K, valBK);
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

        // Inner loop over K
        auto innerLoop = scf::ForOp::create(builder, loc, zero, valBK, one);
        builder.setInsertionPointToStart(innerLoop.getBody());
        Value kInner = innerLoop.getInductionVar();

        // Load regA and regB
        auto regALoop = scf::ForOp::create(builder, loc, zero, valTM, one);
        builder.setInsertionPointToStart(regALoop.getBody());
        Value iA = regALoop.getInductionVar();
        Value aSmRow = arith::AddIOp::create(
            builder, loc, arith::MulIOp::create(builder, loc, ty, valTM), iA);
        Value sA = memref::LoadOp::create(builder, loc, smemA,
                                          ValueRange{aSmRow, kInner});
        memref::StoreOp::create(builder, loc, sA, regA, ValueRange{iA});
        builder.setInsertionPointAfter(regALoop);

        auto regBLoop = scf::ForOp::create(builder, loc, zero, valTN, one);
        builder.setInsertionPointToStart(regBLoop.getBody());
        Value jB = regBLoop.getInductionVar();
        Value bSmCol = arith::AddIOp::create(
            builder, loc, arith::MulIOp::create(builder, loc, tx, valTN), jB);
        Value sB = memref::LoadOp::create(builder, loc, smemB,
                                          ValueRange{kInner, bSmCol});
        memref::StoreOp::create(builder, loc, sB, regB, ValueRange{jB});
        builder.setInsertionPointAfter(regBLoop);

        // Loop over TM x TN
        auto tmLoop = scf::ForOp::create(builder, loc, zero, valTM, one);
        builder.setInsertionPointToStart(tmLoop.getBody());
        Value i = tmLoop.getInductionVar();
        Value aValReg =
            memref::LoadOp::create(builder, loc, regA, ValueRange{i});

        auto tnLoop = scf::ForOp::create(builder, loc, zero, valTN, one);
        builder.setInsertionPointToStart(tnLoop.getBody());
        Value j = tnLoop.getInductionVar();
        Value bValReg =
            memref::LoadOp::create(builder, loc, regB, ValueRange{j});

        Value sMul = arith::MulFOp::create(builder, loc, aValReg, bValReg);
        Value curAcc =
            memref::LoadOp::create(builder, loc, threadAcc, ValueRange{i, j});
        Value nextAcc = arith::AddFOp::create(builder, loc, curAcc, sMul);
        memref::StoreOp::create(builder, loc, nextAcc, threadAcc,
                                ValueRange{i, j});

        builder.setInsertionPointAfter(tnLoop);
        builder.setInsertionPointAfter(tmLoop);

        builder.setInsertionPointAfter(innerLoop);

        gpu::BarrierOp::create(builder, loc);

        builder.setInsertionPointAfter(loopK);

        // Store results
        auto storeLoopI = scf::ForOp::create(builder, loc, zero, valTM, one);
        builder.setInsertionPointToStart(storeLoopI.getBody());
        Value sI = storeLoopI.getInductionVar();
        Value sRow = arith::AddIOp::create(builder, loc, threadRowStart, sI);
        Value sRowCond = arith::CmpIOp::create(
            builder, loc, arith::CmpIPredicate::ult, sRow, M);

        auto storeLoopJ = scf::ForOp::create(builder, loc, zero, valTN, one);
        builder.setInsertionPointToStart(storeLoopJ.getBody());
        Value sJ = storeLoopJ.getInductionVar();
        Value sCol = arith::AddIOp::create(builder, loc, threadColStart, sJ);
        Value sColCond = arith::CmpIOp::create(
            builder, loc, arith::CmpIPredicate::ult, sCol, N);

        Value sInBounds =
            arith::AndIOp::create(builder, loc, sRowCond, sColCond);

        auto storeCIf = scf::IfOp::create(builder, loc, sInBounds,
                                          /*withElseRegion=*/false);
        builder.setInsertionPointToStart(&storeCIf.getThenRegion().front());
        Value finalAcc =
            memref::LoadOp::create(builder, loc, threadAcc, ValueRange{sI, sJ});
        memref::StoreOp::create(builder, loc, finalAcc, C,
                                ValueRange{sRow, sCol});

        builder.setInsertionPointAfter(storeCIf);
        builder.setInsertionPointAfter(storeLoopJ);
        builder.setInsertionPointAfter(storeLoopI);

        gpu::TerminatorOp::create(builder, loc);

        builder.setInsertionPointAfter(launchOp);
        gemmOp.erase();
      } else if (optLevel == 6) {
        // Level 6: Vectorization
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

        builder.setInsertionPointToStart(&launchOp.getBody().front());

        auto smemSpace = gpu::AddressSpaceAttr::get(
            builder.getContext(), gpu::AddressSpace::Workgroup);
        auto f32Type = builder.getF32Type();
        auto vec4Type = VectorType::get({4}, f32Type);

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

        // Thread accumulators
        MemRefType regType = MemRefType::get({TM, TN}, f32Type);
        Value threadAcc = memref::AllocaOp::create(builder, loc, regType);

        MemRefType regAType = MemRefType::get({TM}, f32Type);
        MemRefType regBType = MemRefType::get({TN}, f32Type);
        Value regA = memref::AllocaOp::create(builder, loc, regAType);
        Value regB = memref::AllocaOp::create(builder, loc, regBType);

        // Initialize accumulators from C
        auto initLoopI = scf::ForOp::create(builder, loc, zero, valTM, one);
        builder.setInsertionPointToStart(initLoopI.getBody());
        Value iInit = initLoopI.getInductionVar();
        Value rowInit =
            arith::AddIOp::create(builder, loc, threadRowStart, iInit);
        Value rowCondInit = arith::CmpIOp::create(
            builder, loc, arith::CmpIPredicate::ult, rowInit, M);

        auto initLoopJ = scf::ForOp::create(builder, loc, zero, valTN, one);
        builder.setInsertionPointToStart(initLoopJ.getBody());
        Value jInit = initLoopJ.getInductionVar();
        Value colInit =
            arith::AddIOp::create(builder, loc, threadColStart, jInit);

        Value colCondInit = arith::CmpIOp::create(
            builder, loc, arith::CmpIPredicate::ult, colInit, N);
        Value inBoundsInit =
            arith::AndIOp::create(builder, loc, rowCondInit, colCondInit);

        auto loadCIf = scf::IfOp::create(builder, loc, f32Type, inBoundsInit,
                                         /*withElseRegion=*/true);
        builder.setInsertionPointToStart(&loadCIf.getThenRegion().front());
        Value cVal = memref::LoadOp::create(builder, loc, C,
                                            ValueRange{rowInit, colInit});
        scf::YieldOp::create(builder, loc, cVal);
        builder.setInsertionPointToStart(&loadCIf.getElseRegion().front());
        Value zeroF32 = arith::ConstantOp::create(builder, loc,
                                                  builder.getF32FloatAttr(0.0));
        scf::YieldOp::create(builder, loc, zeroF32);

        builder.setInsertionPointAfter(loadCIf);
        memref::StoreOp::create(builder, loc, loadCIf.getResult(0), threadAcc,
                                ValueRange{iInit, jInit});

        builder.setInsertionPointAfter(initLoopJ);
        builder.setInsertionPointAfter(initLoopI);

        // Outer K loop
        auto loopK = scf::ForOp::create(builder, loc, zero, K, valBK);
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

        Value aVecVal = vector::LoadOp::create(
            builder, loc, vec4Type, A, ValueRange{globalARow, globalACol});
        vector::StoreOp::create(builder, loc, aVecVal, smemA,
                                ValueRange{aRow, aCol});

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

        // Inner loop over K
        auto innerLoop = scf::ForOp::create(builder, loc, zero, valBK, one);
        builder.setInsertionPointToStart(innerLoop.getBody());
        Value kInner = innerLoop.getInductionVar();

        // Load regA and regB
        auto regALoop = scf::ForOp::create(builder, loc, zero, valTM, one);
        builder.setInsertionPointToStart(regALoop.getBody());
        Value iA = regALoop.getInductionVar();
        Value aSmRow = arith::AddIOp::create(
            builder, loc, arith::MulIOp::create(builder, loc, ty, valTM), iA);
        Value sA = memref::LoadOp::create(builder, loc, smemA,
                                          ValueRange{aSmRow, kInner});
        memref::StoreOp::create(builder, loc, sA, regA, ValueRange{iA});
        builder.setInsertionPointAfter(regALoop);

        auto regBLoop = scf::ForOp::create(builder, loc, zero, valTN, one);
        builder.setInsertionPointToStart(regBLoop.getBody());
        Value jB = regBLoop.getInductionVar();
        Value bSmCol = arith::AddIOp::create(
            builder, loc, arith::MulIOp::create(builder, loc, tx, valTN), jB);
        Value sB = memref::LoadOp::create(builder, loc, smemB,
                                          ValueRange{kInner, bSmCol});
        memref::StoreOp::create(builder, loc, sB, regB, ValueRange{jB});
        builder.setInsertionPointAfter(regBLoop);

        // Loop over TM x TN
        auto tmLoop = scf::ForOp::create(builder, loc, zero, valTM, one);
        builder.setInsertionPointToStart(tmLoop.getBody());
        Value i = tmLoop.getInductionVar();
        Value aValReg =
            memref::LoadOp::create(builder, loc, regA, ValueRange{i});

        auto tnLoop = scf::ForOp::create(builder, loc, zero, valTN, one);
        builder.setInsertionPointToStart(tnLoop.getBody());
        Value j = tnLoop.getInductionVar();
        Value bValReg =
            memref::LoadOp::create(builder, loc, regB, ValueRange{j});

        Value sMul = arith::MulFOp::create(builder, loc, aValReg, bValReg);
        Value curAcc =
            memref::LoadOp::create(builder, loc, threadAcc, ValueRange{i, j});
        Value nextAcc = arith::AddFOp::create(builder, loc, curAcc, sMul);
        memref::StoreOp::create(builder, loc, nextAcc, threadAcc,
                                ValueRange{i, j});

        builder.setInsertionPointAfter(tnLoop);
        builder.setInsertionPointAfter(tmLoop);

        builder.setInsertionPointAfter(innerLoop);

        gpu::BarrierOp::create(builder, loc);

        builder.setInsertionPointAfter(loopK);

        // Store results
        auto storeLoopI = scf::ForOp::create(builder, loc, zero, valTM, one);
        builder.setInsertionPointToStart(storeLoopI.getBody());
        Value sI = storeLoopI.getInductionVar();
        Value sRow = arith::AddIOp::create(builder, loc, threadRowStart, sI);
        Value sRowCond = arith::CmpIOp::create(
            builder, loc, arith::CmpIPredicate::ult, sRow, M);

        auto storeLoopJ = scf::ForOp::create(builder, loc, zero, valTN, one);
        builder.setInsertionPointToStart(storeLoopJ.getBody());
        Value sJ = storeLoopJ.getInductionVar();
        Value sCol = arith::AddIOp::create(builder, loc, threadColStart, sJ);
        Value sColCond = arith::CmpIOp::create(
            builder, loc, arith::CmpIPredicate::ult, sCol, N);

        Value sInBounds =
            arith::AndIOp::create(builder, loc, sRowCond, sColCond);

        auto storeCIf = scf::IfOp::create(builder, loc, sInBounds,
                                          /*withElseRegion=*/false);
        builder.setInsertionPointToStart(&storeCIf.getThenRegion().front());
        Value finalAcc =
            memref::LoadOp::create(builder, loc, threadAcc, ValueRange{sI, sJ});
        memref::StoreOp::create(builder, loc, finalAcc, C,
                                ValueRange{sRow, sCol});

        builder.setInsertionPointAfter(storeCIf);
        builder.setInsertionPointAfter(storeLoopJ);
        builder.setInsertionPointAfter(storeLoopI);

        gpu::TerminatorOp::create(builder, loc);

        builder.setInsertionPointAfter(launchOp);
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
