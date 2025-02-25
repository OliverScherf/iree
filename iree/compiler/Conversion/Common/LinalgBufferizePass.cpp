// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

//===- LinalgBufferizePass.cpp.cpp - Pass to bufferize Linalg on tensors --===//
//
// Pass to convert from Linalg ops on tensors to Linalg ops on buffers.
// This just inserts AllocOp to address space 0 that can be later hoisted,
// promoted and generally rewritten to the desired backend.
//
// TODO(nicolasvasilache): the implementation of this pass is unnecessarily
// convoluted due to asymmetries arising from tie_shape weirdness. Revisit once
// this abstraction is replaced.
//
//===----------------------------------------------------------------------===//

#include "iree/compiler/Conversion/CodegenUtils/FunctionUtils.h"
#include "iree/compiler/Conversion/Common/Passes.h"
#include "iree/compiler/Conversion/Common/Transforms.h"
#include "iree/compiler/Dialect/Flow/IR/FlowOps.h"
#include "iree/compiler/Dialect/Flow/IR/FlowTypes.h"
#include "iree/compiler/Dialect/HAL/IR/HALOps.h"
#include "iree/compiler/Dialect/IREE/IR/IREEDialect.h"
#include "iree/compiler/Dialect/IREE/IR/IREEOps.h"
#include "iree/compiler/Dialect/Shape/IR/ShapeOps.h"
#include "llvm/ADT/TypeSwitch.h"
#include "mlir/Dialect/Linalg/IR/LinalgOps.h"
#include "mlir/Dialect/MemRef/IR/MemRef.h"
#include "mlir/Dialect/SCF/SCF.h"
#include "mlir/Dialect/Vector/VectorOps.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/Matchers.h"
#include "mlir/IR/Value.h"
#include "mlir/Pass/Pass.h"

#define DEBUG_TYPE "iree-codegen-linalg-bufferize"

namespace mlir {
namespace iree_compiler {

//===----------------------------------------------------------------------===//
// Utility functions.
//===----------------------------------------------------------------------===//

static MemRefType getMemrefTypeForTensor(RankedTensorType tensorType,
                                         ArrayRef<AffineMap> layout = {},
                                         unsigned memorySpace = 0) {
  return MemRefType::get(tensorType.getShape(), tensorType.getElementType(),
                         layout, memorySpace);
}

// Transfer all `dim` ops on `tensor` to `memref`.
static void transferShapeOpsToMemref(OpBuilder &b, Value tensor, Value memref,
                                     BlockAndValueMapping &bvm) {
  for (OpOperand &opOperand : llvm::make_early_inc_range(tensor.getUses())) {
    if (isa<memref::DimOp>(opOperand.getOwner())) {
      opOperand.set(memref);
      continue;
    }
    if (auto flowTieShapeOp =
            dyn_cast<IREE::Flow::DispatchTieShapeOp>(opOperand.getOwner())) {
      OpBuilder::InsertionGuard g(b);
      b.setInsertionPoint(flowTieShapeOp);
      auto tieShapeOp =
          b.create<Shape::TieShapeOp>(flowTieShapeOp.getLoc(), memref.getType(),
                                      memref, flowTieShapeOp.shape());
      bvm.map(flowTieShapeOp.getResult(), tieShapeOp.getResult());
      continue;
    }
  }
}

/// Creates a subview operation given the `src`, `offsets`, `sizes` and
/// `strides`. Handles the corner case where the `offsets`, `sizes` and
/// `strides` are empty in which case just forward the `src` value.
/// TODO(ataei): Instead create memref.subview %v [][][] folder.
static Value createSubviewOp(OpBuilder &b, Location loc, Value src,
                             ArrayRef<OpFoldResult> offsets,
                             ArrayRef<OpFoldResult> sizes,
                             ArrayRef<OpFoldResult> strides) {
  if (offsets.empty() && sizes.empty() && strides.empty()) return src;
  return b.create<memref::SubViewOp>(loc, src, offsets, sizes, strides);
}

//===----------------------------------------------------------------------===//
// Bufferization helper functions using BlockAndValueMapping.
//===----------------------------------------------------------------------===//

// Non-conversion equivalent of the core MLIR Linalg bufferization patterns.
// Allocate the output buffers for the bufferized Linalg op to write into.
// If the tensor is an init tensor, we additionally copy the original value into
// the newly allocated buffer.
static LogicalResult allocateBuffersForResults(
    OpBuilder &b, Location loc, WorkgroupMemoryAllocationFn allocationFn,
    linalg::LinalgOp op, SmallVectorImpl<Value> &resultBuffers,
    BlockAndValueMapping &bvm) {
  // Lazily compute loopRanges.
  SmallVector<Range, 4> loopRanges;

  assert(op.getNumOutputs() == op->getNumResults());
  for (auto en : llvm::enumerate(op->getResultTypes())) {
    size_t resultIndex = en.index();
    Value outTensor = op.getOutput(resultIndex);
    Value resultTensor = op->getResult(en.index());

    // If output tensor was produced by a LinalgOp, just reuse the buffer.
    // TODO(nicolasvasilache): this may be too brutal and we may prefer to leave
    // this decision to a copy + alloc removal pass.
    if (outTensor.getDefiningOp<linalg::LinalgOp>()) {
      Value outBuffer = bvm.lookup(outTensor);
      bvm.map(resultTensor, outBuffer);
      resultBuffers.push_back(outBuffer);
      continue;
    }

    // If resultTensor already has a buffer, just use that.
    Value alloc = bvm.lookupOrNull(resultTensor);
    if (!alloc) {
      Type resultType = en.value();
      auto tensorType = resultType.dyn_cast<RankedTensorType>();
      auto tensorShape = tensorType.getShape();
      SmallVector<Value, 4> dynOperands;
      for (auto dim : llvm::enumerate(tensorShape)) {
        Value dimTensor = bvm.lookupOrNull(outTensor);
        if (!dimTensor) dimTensor = outTensor;
        if (dim.value() == TensorType::kDynamicSize) {
          dynOperands.push_back(
              b.createOrFold<memref::DimOp>(loc, dimTensor, dim.index()));
        }
      }
      alloc = allocationFn(b, loc, tensorShape, tensorType.getElementType(),
                           dynOperands);
      bvm.map(resultTensor, alloc);
    }
    resultBuffers.push_back(alloc);

    // Additionally, if the output buffer is used, clone its value for now.  The
    // method `payloadUsesValueFromOutputOperandIndex` only works on named ops
    // that have a region. Named ops like `conv`, etc. that are manually defined
    // do not have this generated by default. So for now, just handled these
    // manually defined ops specifically.
    if (!isa<linalg::FillOp>(op.getOperation()) &&
        op.payloadUsesValueFromOutputOperandIndex(resultIndex)) {
      b.create<linalg::CopyOp>(loc, bvm.lookup(outTensor), alloc);
    }
  }
  for (auto it : llvm::zip(op->getResults(), resultBuffers)) {
    transferShapeOpsToMemref(b, std::get<0>(it), std::get<1>(it), bvm);
  }
  return success();
}

// Non-conversion equivalent of the core MLIR Linalg bufferization patterns.
static LogicalResult finalizeBufferAllocation(OpBuilder &b, linalg::LinalgOp op,
                                              ValueRange inputs,
                                              ValueRange outputs,
                                              BlockAndValueMapping &bvm) {
  SmallVector<Value, 8> newOperands = inputs;
  newOperands.append(outputs.begin(), outputs.end());
  auto otherOperands =
      llvm::map_range(op.getAssumedNonShapedOperands(),
                      [&bvm](Value v) { return bvm.lookupOrDefault(v); });
  newOperands.append(otherOperands.begin(), otherOperands.end());
  Location loc = op.getLoc();
  op.clone(b, loc, {}, newOperands);

  // Replace the results of the old op with the new output buffers.
  for (auto result : llvm::enumerate(op.getOperation()->getResults())) {
    Value resultValue = result.value();
    Value resultBuffer = bvm.lookup(resultValue);
    if (resultBuffer != outputs[result.index()]) {
      b.create<linalg::CopyOp>(loc, outputs[result.index()], resultBuffer);
    }
  }
  return success();
}

/// Generic conversion pattern that matches any linalg::LinalgOp. This avoids
/// template instantiating one pattern for each linalg::LinalgOp.
static LogicalResult convertAnyLinalgOp(
    OpBuilder &b, WorkgroupMemoryAllocationFn allocationFn, linalg::LinalgOp op,
    BlockAndValueMapping &bvm) {
  // Skip linalg ops inserted by this pass.
  if (op.hasBufferSemantics()) return success();

  OpBuilder::InsertionGuard g(b);
  b.setInsertionPoint(op);
  Location loc = op.getLoc();
  SmallVector<Value, 2> newInputBuffers;
  newInputBuffers.reserve(op.getNumInputs());
  for (Value v : op.getInputs()) {
    newInputBuffers.push_back(bvm.lookup(v));
  }
  SmallVector<Value, 2> newOutputBuffers;
  if (failed(allocateBuffersForResults(b, loc, allocationFn, op,
                                       newOutputBuffers, bvm))) {
    LLVM_DEBUG(llvm::dbgs()
               << "failed to allocate output buffers for op: " << op << "\n");
    return failure();
  }

  // Delegate to the linalg generic pattern.
  if (auto genericOp = dyn_cast<linalg::GenericOp>(op.getOperation())) {
    return finalizeBufferAllocation(b, genericOp, newInputBuffers,
                                    newOutputBuffers, bvm);
  }

  return finalizeBufferAllocation(b, op, newInputBuffers, newOutputBuffers,
                                  bvm);
}

/// Constants that return tensor types can be handled natively by the
/// backends. Here just provide a cast to memref to bridge the gap from tensors
/// to memrefs.
static LogicalResult convertConstantOp(OpBuilder &b, ConstantOp constantOp,
                                       BlockAndValueMapping &bvm) {
  Value result = constantOp.getResult();
  RankedTensorType tensorType = result.getType().dyn_cast<RankedTensorType>();
  if (!tensorType) return success();
  OpBuilder::InsertionGuard g(b);
  b.setInsertionPointAfter(constantOp);
  auto memrefType = getMemrefTypeForTensor(tensorType);
  Value memref =
      b.create<memref::BufferCastOp>(constantOp.getLoc(), memrefType, result);
  if (Value resultBuffer = bvm.lookupOrNull(result)) {
    // Since this is already remapped to a buffer, copy the data. Note that
    // constant ops are typicaly placed at the beginning of the block; we need
    // to make sure to insert the new copy op after the result buffer, which can
    // be after the constant op.
    b.setInsertionPointAfterValue(resultBuffer);
    b.create<linalg::CopyOp>(constantOp.getLoc(), memref, resultBuffer);
  } else {
    bvm.map(result, memref);
  }
  return success();
}

/// Converts a linalg.init_tensor op to memref.alloc op. This provides a shaped
/// operand for pooling ops. The op will be deleted after going to loops.
static LogicalResult convertInitTensorOp(
    OpBuilder &b, WorkgroupMemoryAllocationFn allocationFn,
    linalg::InitTensorOp initTensorOp, BlockAndValueMapping &bvm) {
  if (bvm.contains(initTensorOp.getResult())) return success();
  RankedTensorType tensorType = initTensorOp.getType();
  OpBuilder::InsertionGuard g(b);
  b.setInsertionPointAfter(initTensorOp);
  Value alloc = allocationFn(b, initTensorOp.getLoc(), tensorType.getShape(),
                             tensorType.getElementType(),
                             llvm::to_vector<4>(initTensorOp.sizes()));
  bvm.map(initTensorOp.getResult(), alloc);
  return success();
}

/// Walks the use-def chain and see if this value comes from a read-only tensor.
static bool isFromReadOnlyTensor(Value v) {
  auto definingOp = v.getDefiningOp();
  if (!definingOp) return false;
  return TypeSwitch<Operation *, bool>(definingOp)
      .Case<ConstantOp>([&](ConstantOp constantOp) { return true; })
      .Case<linalg::TensorReshapeOp>([&](linalg::TensorReshapeOp reshapeOp) {
        return isFromReadOnlyTensor(reshapeOp.src());
      })
      .Case<SubTensorOp>([&](SubTensorOp subTensorOp) {
        return isFromReadOnlyTensor(subTensorOp.source());
      })
      .Case<IREE::Flow::DispatchTensorLoadOp>(
          [&](IREE::Flow::DispatchTensorLoadOp loadOp) {
            return loadOp.source()
                       .getType()
                       .cast<IREE::Flow::DispatchTensorType>()
                       .getAccess() == IREE::Flow::TensorAccess::ReadOnly;
          })
      .Default([&](Operation *op) { return false; });
}

/// Avoids creating an allocation if the result tensor can just be aliased to
/// use the same buffer (`inputBuffer`) that `srcTensor` is mapped to. This can
/// be done if `srcTensor` has a single use, which is the operation which is
/// being converted to buffers.
/// Note that the mapping for `srcTensor` need not be mapped to `inputBuffer`
/// directly. It could also be mapped to an alias of the `inputBuffer.
static LogicalResult createAliasingBufferOrAllocationForResult(
    OpBuilder &b, Location loc, WorkgroupMemoryAllocationFn allocationFn,
    Value srcTensor, Value inputBuffer, Value resultTensor,
    ArrayRef<Value> allocationDynamicDims, BlockAndValueMapping &bvm) {
  // Case 1 : If result tensor is already mapped to a buffer just copy the
  // value.
  if (Value outputBuffer = bvm.lookupOrNull(resultTensor)) {
    if (inputBuffer != outputBuffer) {
      b.create<linalg::CopyOp>(loc, inputBuffer, outputBuffer);
    }
    return success();
  }
  // Case 2: If the input tensor has only one use (this operation) or is from a
  // read-only tensor, then no need to create a copy either.
  if (srcTensor.hasOneUse() || isFromReadOnlyTensor(srcTensor)) {
    bvm.map(resultTensor, inputBuffer);
    return success();
  }
  // Fallback is to create an allocation and copy the output.
  MemRefType inputBufferType = inputBuffer.getType().cast<MemRefType>();
  assert(allocationDynamicDims.size() ==
         static_cast<size_t>(inputBufferType.getRank()));
  Value alloc = allocationFn(
      b, loc, SmallVector<int64_t, 4>(inputBufferType.getRank(), -1),
      inputBufferType.getElementType(), allocationDynamicDims);
  b.create<linalg::CopyOp>(loc, inputBuffer, alloc);
  bvm.map(resultTensor, alloc);
  return success();
}

/// Converts a `linalg.tensor_reshape` operation to a `linalg.reshape`
/// operation.
static LogicalResult convertTensorReshapeOp(
    OpBuilder &b, WorkgroupMemoryAllocationFn allocationFn,
    linalg::TensorReshapeOp op, BlockAndValueMapping &bvm) {
  OpBuilder::InsertionGuard g(b);
  b.setInsertionPoint(op);
  Location loc = op.getLoc();
  Value srcTensor = op.src();
  RankedTensorType srcTensorType = op.getSrcType();
  Value resultTensor = op.result();
  RankedTensorType resultTensorType = op.getResultType();
  Value inputBuffer = bvm.lookup(srcTensor);
  MemRefType inputBufferType = inputBuffer.getType().cast<MemRefType>();
  // Create the reshape op.
  auto reshapeSrcType = getMemrefTypeForTensor(
      srcTensorType, {}, inputBufferType.getMemorySpaceAsInt());
  Value reshapeSrc =
      b.createOrFold<memref::CastOp>(loc, inputBuffer, reshapeSrcType);
  auto reshapeResultType = getMemrefTypeForTensor(
      resultTensorType, {}, inputBufferType.getMemorySpaceAsInt());
  Value bufferReshape = b.create<linalg::ReshapeOp>(
      loc, reshapeResultType, reshapeSrc, op.reassociation());
  SmallVector<SmallVector<Value>> reshapeResultShape;
  if (failed(op.reifyReturnTypeShapesPerResultDim(b, reshapeResultShape)) ||
      reshapeResultShape.size() != 1) {
    return op.emitError("failed to get shape of result");
  }
  return createAliasingBufferOrAllocationForResult(
      b, loc, allocationFn, srcTensor, bufferReshape, resultTensor,
      reshapeResultShape[0], bvm);
}

static SmallVector<int64_t, 4> extractFromI64ArrayAttr(ArrayAttr attr) {
  return llvm::to_vector<4>(llvm::map_range(attr, [](Attribute a) -> int64_t {
    return a.cast<IntegerAttr>().getInt();
  }));
}

/// Converts a `subtensor` operation to a `subview` operation.
static LogicalResult convertSubTensorOp(
    OpBuilder &b, WorkgroupMemoryAllocationFn allocationFn, SubTensorOp op,
    BlockAndValueMapping &bvm) {
  OpBuilder::InsertionGuard g(b);
  b.setInsertionPoint(op);
  Location loc = op.getLoc();
  Value srcTensor = op.source();
  Value resultTensor = op.result();
  Value inputBuffer = bvm.lookup(srcTensor);
  MemRefType inputBufferType = inputBuffer.getType().cast<MemRefType>();

  auto subViewResultType = memref::SubViewOp::inferResultType(
      inputBufferType, extractFromI64ArrayAttr(op.static_offsets()),
      extractFromI64ArrayAttr(op.static_sizes()),
      extractFromI64ArrayAttr(op.static_strides()));
  auto subViewOp = b.create<memref::SubViewOp>(
      loc, subViewResultType, inputBuffer, op.offsets(), op.sizes(),
      op.strides(), op.static_offsets(), op.static_sizes(),
      op.static_strides());
  auto allocationDynamicSizes = llvm::to_vector<4>(
      llvm::map_range(subViewOp.getOrCreateRanges(b, loc), [](Range range) {
        assert(matchPattern(range.stride, m_One()) &&
               "unhandled non-unit stride");
        return range.size;
      }));
  return createAliasingBufferOrAllocationForResult(
      b, loc, allocationFn, srcTensor, subViewOp, resultTensor,
      allocationDynamicSizes, bvm);
}

/// Converts a `subtensor_insert` operation to buffers by
/// - Allocating a buffer for the result (if needed), and copying the
///   destination value into this buffer.
/// - Copying the source values into a subview of the result buffer.
static LogicalResult convertSubTensorInsertOp(
    OpBuilder &b, WorkgroupMemoryAllocationFn allocationFn,
    SubTensorInsertOp op, BlockAndValueMapping &bvm) {
  OpBuilder::InsertionGuard g(b);
  b.setInsertionPoint(op);
  Location loc = op.getLoc();
  Value dest = op.dest();
  Value inputBuffer = bvm.lookup(dest);
  SmallVector<Value> allocationDynamicSizes;
  int64_t rank = inputBuffer.getType().cast<ShapedType>().getRank();
  for (auto dim : llvm::seq<int64_t>(0, rank)) {
    allocationDynamicSizes.push_back(
        b.createOrFold<memref::DimOp>(loc, inputBuffer, dim));
  }
  if (failed(createAliasingBufferOrAllocationForResult(
          b, loc, allocationFn, dest, inputBuffer, op.getResult(),
          allocationDynamicSizes, bvm))) {
    return failure();
  }

  Value source = op.source();
  Value outputBuffer = bvm.lookup(op.result());
  Value sourceBuffer = bvm.lookup(source);
  auto subViewOp = createSubviewOp(b, loc, outputBuffer, op.getMixedOffsets(),
                                   op.getMixedSizes(), op.getMixedStrides());
  b.create<linalg::CopyOp>(loc, sourceBuffer, subViewOp);
  return success();
}

/// Converts a `tensor.extract` operation into a `load`.
static LogicalResult convertTensorExtractOp(OpBuilder &b, tensor::ExtractOp op,
                                            BlockAndValueMapping &bvm) {
  OpBuilder::InsertionGuard g(b);
  b.setInsertionPoint(op);
  Value inputBuffer = bvm.lookup(op.tensor());
  Value load =
      b.createOrFold<memref::LoadOp>(op.getLoc(), inputBuffer, op.indices());
  bvm.map(op.result(), load);
  return success();
}

static LogicalResult convertTransferOp(OpBuilder &b,
                                       WorkgroupMemoryAllocationFn allocationFn,
                                       VectorTransferOpInterface op,
                                       BlockAndValueMapping &bvm) {
  if (op.getShapedType().isa<MemRefType>()) return failure();
  assert(op->getNumResults() == 1);
  Value outputTensor = op->getResult(0);
  OpBuilder::InsertionGuard g(b);
  b.setInsertionPoint(op);
  Location loc = op.getLoc();
  Value newInputBuffer = bvm.lookup(op.source());
  if (auto tensorType =
          op->getResult(0).getType().dyn_cast<RankedTensorType>()) {
    // If the op return a Tensor allocate a buffer for the returned value.
    auto tensorShape = tensorType.getShape();
    SmallVector<Value, 4> dynOperands;
    for (size_t idx : llvm::seq(size_t(0), tensorShape.size())) {
      if (tensorType.isDynamicDim(idx)) {
        Value tensor = bvm.lookupOrNull(outputTensor);
        if (!tensor) tensor = outputTensor;
        dynOperands.push_back(b.createOrFold<memref::DimOp>(loc, tensor, idx));
      }
    }
    auto alloc = allocationFn(b, loc, tensorShape, tensorType.getElementType(),
                              dynOperands);
    bvm.map(op->getResult(0), alloc);
    transferShapeOpsToMemref(b, op->getResult(0), alloc, bvm);
  }

  // Replace the tensor operand.
  if (auto readOp = dyn_cast<vector::TransferReadOp>(op.getOperation())) {
    readOp.sourceMutable().assign(newInputBuffer);
  } else {
    auto writeOp = cast<vector::TransferWriteOp>(op.getOperation());
    // Create a new transfer_write on buffer that doesn't have a return value.
    // Leave the previous transfer_write to dead code as it still has uses at
    // this point.
    b.create<vector::TransferWriteOp>(
        loc, writeOp.vector(), newInputBuffer, writeOp.indices(),
        writeOp.permutation_map(), writeOp.mask(),
        writeOp.in_bounds() ? *writeOp.in_bounds() : ArrayAttr());
  }
  return success();
}

// Extract int64_t values from the assumed ArrayAttr of IntegerAttr.
static SmallVector<int64_t, 4> extractFromI64ArrayAttr(Attribute attr) {
  return llvm::to_vector<4>(llvm::map_range(
      attr.cast<ArrayAttr>(),
      [](Attribute a) -> int64_t { return a.cast<IntegerAttr>().getInt(); }));
}

LogicalResult convertInterfaceLoadTensorOp(
    OpBuilder &b, IREE::Flow::DispatchTensorLoadOp loadOp,
    BlockAndValueMapping &bvm) {
  OpBuilder::InsertionGuard g(b);
  b.setInsertionPoint(loadOp);
  Location loc = loadOp.getLoc();
  Value memref = bvm.lookup(loadOp.source());
  Value res = createSubviewOp(b, loc, memref, loadOp.getMixedOffsets(),
                              loadOp.getMixedSizes(), loadOp.getMixedStrides());

  bvm.map(loadOp.result(), res);
  transferShapeOpsToMemref(b, loadOp.result(), res, bvm);
  return success();
}

/// For a given store-like `op` that is to be replaced, find the insertion point
/// in the same block earliest possible when
/// - the replacement op uses values in `usedValues`, so has to be inserted
///   after the ops that define these.
/// - The op needs to be inserted before `insertBefore` (which is in the same
/// block). Return nullptr all other times.
static Operation *getInsertionPointForReplacementStoreOp(
    Operation *op, Operation *insertBefore, ArrayRef<Value> usedValues) {
  if (op->getBlock() != insertBefore->getBlock()) return nullptr;
  Operation *insertAfter = nullptr;
  for (auto value : usedValues) {
    Operation *definingOp = value.getDefiningOp();
    if (!definingOp || definingOp->getBlock() != insertBefore->getBlock())
      continue;
    if (!insertAfter || insertAfter->isBeforeInBlock(definingOp))
      insertAfter = definingOp;
  }
  // All defining ops are outside of the block, so just insert at the start of
  // the block.
  if (!insertAfter) return &(op->getBlock()->front());
  if (insertAfter->isBeforeInBlock(insertBefore))
    return insertAfter->getNextNode();
  return nullptr;
}

/// For cases where the value operand of the `storeOp` is produced by a
/// LinalgOp, create the subview operation that can be used by the op itself to
/// store the result into directly. This avoids an extra allocation + copies.
LogicalResult preProcessInterfaceStoreTensorOp(
    OpBuilder &b, IREE::Flow::DispatchTensorStoreOp storeOp,
    BlockAndValueMapping &bvm) {
  // Find the insertion point for the subview.
  SmallVector<Value, 4> operandsOfSubviewOp;
  operandsOfSubviewOp.push_back(bvm.lookup(storeOp.target()));
  operandsOfSubviewOp.append(storeOp.offsets().begin(),
                             storeOp.offsets().end());
  operandsOfSubviewOp.append(storeOp.sizes().begin(), storeOp.sizes().end());
  operandsOfSubviewOp.append(storeOp.strides().begin(),
                             storeOp.strides().end());
  Operation *insertionPoint = getInsertionPointForReplacementStoreOp(
      storeOp.getOperation(), storeOp.value().getDefiningOp(),
      operandsOfSubviewOp);
  if (!insertionPoint) return success();
  OpBuilder::InsertionGuard g(b);
  b.setInsertionPoint(insertionPoint);
  Value subview =
      createSubviewOp(b, storeOp.getLoc(), bvm.lookup(storeOp.target()),
                      storeOp.getMixedOffsets(), storeOp.getMixedSizes(),
                      storeOp.getMixedStrides());
  bvm.map(storeOp.value(), subview);
  return success();
}

/// Pre process linalg operations (on tensors) to propagate buffer assignment
/// from results to operands wherever possible.
LogicalResult preProcessLinalgOps(OpBuilder &b, linalg::LinalgOp op,
                                  BlockAndValueMapping &bvm) {
  if (!op.hasTensorSemantics()) return success();

  for (auto en :
       llvm::zip(op.getOperation()->getResults(), op.getOutputTensors())) {
    Value resultTensor = std::get<0>(en);
    Value outTensor = std::get<1>(en);
    unsigned resultIndex = resultTensor.cast<OpResult>().getResultNumber();
    Value resultBuffer = bvm.lookupOrNull(resultTensor);

    // If the result is mapped to a buffer, the corresponding output tensor can
    // be mapped to the same buffer to make this an inplace update.
    if (resultBuffer && outTensor.hasOneUse()) {
      bvm.map(outTensor, resultBuffer);
    }

    // If the output tensor is not actually used (for initialization) by this
    // op, we can reuse the result tensor's buffer for some operands.
    if (!op.payloadUsesValueFromOutputOperandIndex(resultIndex)) {
      for (auto en : llvm::enumerate(op.getInputTensors())) {
        Value operand = en.value();
        auto producerOp = operand.getDefiningOp<linalg::LinalgOp>();
        if (producerOp && operand.hasOneUse() &&
            operand.getType() == resultTensor.getType() &&
            op.getInputIndexingMap(en.index()) ==
                op.getOutputIndexingMap(resultIndex)) {
          bvm.map(operand, resultBuffer);
          break;
        }
      }
    }
  }

  return success();
}

// Check if the buffer being copied from and being stored to are the same. If so
// this copy is unnecessary since the output has been updated in place.
bool isRedundantCopy(Value storeTo, Value storeFrom) {
  if (storeTo == storeFrom) return true;
  auto storeFromOp = storeFrom.getDefiningOp<memref::SubViewOp>();
  return storeFromOp && storeFromOp.source() == storeTo;
}

LogicalResult convertInterfaceStoreTensorOp(
    OpBuilder &b, IREE::Flow::DispatchTensorStoreOp storeOp,
    BlockAndValueMapping &bvm) {
  OpBuilder::InsertionGuard g(b);
  b.setInsertionPoint(storeOp);
  Value storeTo = bvm.lookup(storeOp.target());
  Value storeFrom = bvm.lookup(storeOp.value());
  // If the value already has a mapping, it should already have been updated in
  // place by the converted producer.
  if (isRedundantCopy(storeTo, storeFrom)) {
    storeOp->erase();
    return success();
  }

  Value subview =
      createSubviewOp(b, storeOp.getLoc(), storeTo, storeOp.getMixedOffsets(),
                      storeOp.getMixedSizes(), storeOp.getMixedStrides());

  b.create<linalg::CopyOp>(storeOp->getLoc(), storeFrom, subview);
  storeOp->erase();
  return success();
}

namespace {
class LinalgBufferizePass
    : public PassWrapper<LinalgBufferizePass, FunctionPass> {
 public:
  LinalgBufferizePass(WorkgroupMemoryAllocationFn fn) : allocationFn(fn) {}
  void getDependentDialects(DialectRegistry &registry) const override {
    registry.insert<IREEDialect, linalg::LinalgDialect, memref::MemRefDialect,
                    scf::SCFDialect, StandardOpsDialect>();
  }
  void runOnFunction() override;

 private:
  WorkgroupMemoryAllocationFn allocationFn;
};
}  // namespace

// Special handling of dynamic sizes that must tie to InterfaceBindingSubspanOp.
// This is necessary to propagate the InterfaceLoadConstantOp to memrefs.
// In tensor world, the information is carried by TieShape ops.
// TODO(ravishankarm): This needs to be moved to MaterializeInterface pass so
// that here we dont need to deal with tie-shape ops.
static Shape::MakeRankedShapeOp getMakeRankedShapeFromInterface(
    IREE::HAL::InterfaceBindingSubspanOp op) {
  for (Operation *user : op->getUsers()) {
    auto tieOp = dyn_cast<IREE::Flow::DispatchTieShapeOp>(user);
    if (!tieOp) continue;
    auto makeRankedShapeOp =
        tieOp.shape().getDefiningOp<Shape::MakeRankedShapeOp>();
    assert(makeRankedShapeOp);
    return makeRankedShapeOp;
  }
  llvm_unreachable("Expected IREE::Flow::DispatchTieShapeOp of op");
}

void LinalgBufferizePass::runOnFunction() {
  FuncOp funcOp = getFunction();
  MLIRContext *context = &getContext();
  OpBuilder b(context);

  BlockAndValueMapping bvm;

  // First go over all hal.interface.binding.subspan ops and create counterparts
  // working with memrefs.
  funcOp.walk([&](IREE::HAL::InterfaceBindingSubspanOp op) {
    auto shapedType =
        op.getResult().getType().dyn_cast<IREE::Flow::DispatchTensorType>();
    if (!shapedType || !shapedType.hasRank()) return;
    OpBuilder::InsertionGuard g(b);
    b.setInsertionPoint(op);
    // Just change the result type of the InterfaceBindingSubspanOp to form
    // the base buffer.
    auto tensorType =
        op.result().getType().cast<IREE::Flow::DispatchTensorType>();
    auto memRefType =
        MemRefType::get(tensorType.getShape(), tensorType.getElementType());
    auto baseBuffer = b.create<IREE::HAL::InterfaceBindingSubspanOp>(
        op->getLoc(), memRefType, op.binding(), op.byte_offset(),
        op.byte_length());
    bvm.map(op, baseBuffer);
    transferShapeOpsToMemref(b, op.getResult(), baseBuffer.getResult(), bvm);
  });

  if (funcOp
          .walk([&](IREE::Flow::DispatchTensorStoreOp op) -> WalkResult {
            return preProcessInterfaceStoreTensorOp(b, op, bvm);
          })
          .wasInterrupted()) {
    return signalPassFailure();
  }

  /// Walk the linalg operations backwards (if they are all in the same basic
  /// block) to propagate buffer usage backwards to reduce the need for
  /// allocation. This works for simple cases where all the linalg operations
  /// are within the same basic block. Fallback is to create a separate
  /// allocation for the output.
  {
    SmallVector<linalg::LinalgOp, 4> linalgOps;
    SmallVector<Operation *, 4> tiledLoops;
    if (succeeded(getLinalgOps(funcOp, linalgOps, tiledLoops))) {
      for (linalg::LinalgOp op : llvm::reverse(linalgOps)) {
        if (failed(preProcessLinalgOps(b, op, bvm))) {
          return signalPassFailure();
        }
      }
    }
  }

  auto conversionDispatch = [&](Operation *op) -> WalkResult {
    return TypeSwitch<Operation *, LogicalResult>(op)
        .Case<ConstantOp>([&](ConstantOp constantOp) {
          return convertConstantOp(b, constantOp, bvm);
        })
        .Case<IREE::Flow::DispatchTensorLoadOp>(
            [&](IREE::Flow::DispatchTensorLoadOp loadOp) {
              return convertInterfaceLoadTensorOp(b, loadOp, bvm);
            })
        .Case<IREE::Flow::DispatchTensorStoreOp>(
            [&](IREE::Flow::DispatchTensorStoreOp storeOp) {
              return convertInterfaceStoreTensorOp(b, storeOp, bvm);
            })
        .Case<linalg::LinalgOp>([&](linalg::LinalgOp linalgOp) {
          return convertAnyLinalgOp(b, allocationFn, linalgOp, bvm);
        })
        .Case<SubTensorInsertOp>([&](SubTensorInsertOp subTensorInsertOp) {
          return convertSubTensorInsertOp(b, allocationFn, subTensorInsertOp,
                                          bvm);
        })
        .Case<SubTensorOp>([&](SubTensorOp subTensorOp) {
          return convertSubTensorOp(b, allocationFn, subTensorOp, bvm);
        })
        .Case<linalg::TensorReshapeOp>([&](linalg::TensorReshapeOp reshapeOp) {
          return convertTensorReshapeOp(b, allocationFn, reshapeOp, bvm);
        })
        .Case<linalg::InitTensorOp>([&](linalg::InitTensorOp initTensorOp) {
          return convertInitTensorOp(b, allocationFn, initTensorOp, bvm);
        })
        .Case<tensor::ExtractOp>([&](tensor::ExtractOp extractOp) {
          return convertTensorExtractOp(b, extractOp, bvm);
        })
        .Case<VectorTransferOpInterface>(
            [&](VectorTransferOpInterface vectorTransferOp) {
              return convertTransferOp(b, allocationFn, vectorTransferOp, bvm);
            })
        .Default([&](Operation *op) {
          // Replace any scalar remapped operands to the new values.
          // TODO(GH-5013): This is really hacky solution, but gets us past for
          // the time being. This all should be replaced by a pattern.
          for (unsigned i : llvm::seq<unsigned>(0, op->getNumOperands())) {
            Value operand = op->getOperand(i);
            if (operand.getType().isIntOrIndexOrFloat()) {
              Value remappedVal = bvm.lookupOrNull(operand);
              if (remappedVal) op->setOperand(i, remappedVal);
            }
          }
          return success();
        });
  };
  if (funcOp.walk(conversionDispatch).wasInterrupted()) {
    return signalPassFailure();
  }
}

static Value defaultAllocationFn(OpBuilder &builder, Location loc,
                                 ArrayRef<int64_t> staticShape,
                                 Type elementType,
                                 ArrayRef<Value> dynamicSizes) {
  auto allocationType = MemRefType::get(staticShape, elementType);
  return builder.create<memref::AllocOp>(loc, allocationType, dynamicSizes);
}

std::unique_ptr<OperationPass<FuncOp>> createLinalgBufferizePass(
    WorkgroupMemoryAllocationFn allocationFn) {
  return std::make_unique<LinalgBufferizePass>(
      allocationFn ? allocationFn : defaultAllocationFn);
}

static PassRegistration<LinalgBufferizePass> pass(
    "iree-codegen-linalg-bufferize",
    "Convert from to Linalg ops on tensors to buffers",
    [] { return std::make_unique<LinalgBufferizePass>(defaultAllocationFn); });
}  // namespace iree_compiler
}  // namespace mlir
