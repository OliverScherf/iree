// Copyright 2019 Google LLC
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

#include "iree/compiler/Dialect/Flow/IR/FlowOps.h"
#include "iree/compiler/Dialect/HAL/Conversion/FlowToHAL/ConvertFlowToHAL.h"
#include "iree/compiler/Dialect/HAL/IR/HALOps.h"
#include "iree/compiler/Dialect/HAL/IR/HALTypes.h"
#include "iree/compiler/Dialect/HAL/Target/TargetBackend.h"
#include "iree/compiler/Dialect/HAL/Target/TargetRegistry.h"
#include "iree/compiler/Dialect/HAL/Utils/DeviceSwitchBuilder.h"
#include "iree/compiler/Dialect/HAL/Utils/TypeUtils.h"
#include "iree/compiler/Dialect/IREE/IR/IREETypes.h"
#include "iree/compiler/Dialect/Shape/IR/ShapeOps.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/Support/Debug.h"
#include "mlir/Dialect/StandardOps/IR/Ops.h"
#include "mlir/IR/Attributes.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/Matchers.h"
#include "mlir/Transforms/DialectConversion.h"

#define DEBUG_TYPE "iree-hal"

namespace mlir {
namespace iree_compiler {
namespace {

struct BufferRange {
  BufferRange() = default;
  explicit BufferRange(Value buffer) : buffer(buffer) {}

  Value buffer = nullptr;
};

// Allocated buffers used within the stream.
struct BufferSet {
  explicit BufferSet(Value allocator) : allocator(allocator) {}

  // Allocator instance the buffers come from.
  Value allocator = nullptr;

  // All output buffers in the same order as the original results.
  SmallVector<Value, 4> outputBuffers;

  // Maps tensor values within the stream to a buffer range that stores them.
  DenseMap<Value, BufferRange> rangeMap;
};

// HACK: until we are doing buffer allocation we need to pin tied buffers all
// the way up the stream from the outputs.
static void propagateTiedBuffer(BufferSet &bufferSet, Value streamValue,
                                BufferRange bufferRange) {
  Value baseValue = streamValue;
  while (auto definingOp = dyn_cast_or_null<IREE::TiedOpInterface>(
             baseValue.getDefiningOp())) {
    auto tiedValue = definingOp.getTiedResultOperand(baseValue);
    if (!tiedValue) break;
    baseValue = tiedValue;
    bufferSet.rangeMap[baseValue] = bufferRange;
  }
}

// Allocates a buffer for the given stream output value.
// |streamValue| is the Value used within the stream region and
// |externalValue| is the returned value from the stream region in the parent
// block.
static Value allocateOutputBuffer(Value streamValue, Value externalValue,
                                  Value allocator,
                                  ConversionPatternRewriter &rewriter) {
  Location loc = externalValue.getLoc();
  // TODO(benvanik): compute from SSA use-def chain uses.
  IREE::HAL::MemoryTypeBitfield memoryTypes =
      IREE::HAL::MemoryTypeBitfield::DeviceLocal |
      IREE::HAL::MemoryTypeBitfield::HostVisible;
  IREE::HAL::BufferUsageBitfield bufferUsage =
      IREE::HAL::BufferUsageBitfield::All;

  // Compute the allocation size for the value.
  auto elementType = IREE::HAL::getElementTypeValue(
      streamValue.getType().cast<ShapedType>().getElementType());
  if (!elementType) {
    return {};
  }
  auto shape = IREE::HAL::getShapeDims(loc, streamValue, rewriter);
  if (!shape) {
    return {};
  }
  auto allocationSize = rewriter
                            .create<IREE::HAL::AllocatorComputeSizeOp>(
                                loc, allocator, *shape, elementType.getValue())
                            .getResult();

  auto buffer = rewriter
                    .create<IREE::HAL::AllocatorAllocateOp>(
                        loc, IREE::HAL::BufferType::get(rewriter.getContext()),
                        allocator, memoryTypes, bufferUsage, allocationSize)
                    .getResult();

  return buffer;
}

// Allocates all output buffers for the stream and populates the |bufferSet|
// with the new mappings.
static void allocateOutputBuffers(IREE::Flow::ExStreamFragmentOp streamOp,
                                  BufferSet &bufferSet,
                                  ConversionPatternRewriter &rewriter) {
  auto tiedStreamOp = cast<IREE::TiedOpInterface>(streamOp.getOperation());
  auto &entryBlock = streamOp.body().front();

  // Allocate output buffers and replace the original uses with the buffers.
  auto returnOp = cast<IREE::Flow::ReturnOp>(streamOp.body().front().back());
  for (auto result : llvm::enumerate(streamOp.getResults())) {
    auto streamValue = returnOp.getOperand(result.index());
    auto externalValue = result.value();

    // Tied results reuse their operand buffer.
    BufferRange bufferRange;
    auto tiedOperandIndex =
        tiedStreamOp.getTiedResultOperandIndex(result.index());
    if (tiedOperandIndex.hasValue()) {
      LLVM_DEBUG(llvm::dbgs()
                 << "    -- REUSING TIED OPERAND("
                 << tiedOperandIndex.getValue() << ") BUFFER FOR STREAM RESULT("
                 << result.index() << "): " << streamOp << "\n");
      auto operand = entryBlock.getArgument(tiedOperandIndex.getValue());
      bufferRange = bufferSet.rangeMap[operand];
    } else {
      auto buffer = allocateOutputBuffer(streamValue, externalValue,
                                         bufferSet.allocator, rewriter);
      bufferRange = BufferRange{buffer};
    }
    assert(bufferRange.buffer);
    bufferSet.outputBuffers.push_back(bufferRange.buffer);
    bufferSet.rangeMap[externalValue] = bufferRange;
    bufferSet.rangeMap[streamValue] = bufferRange;
    propagateTiedBuffer(bufferSet, streamValue, bufferRange);
  }
}

// Allocates a transient buffer for use entirely within the command buffer.
static Value allocateTransientBuffer(Value streamValue, Value allocator,
                                     ConversionPatternRewriter &rewriter) {
  Location loc = streamValue.getLoc();
  // TODO(benvanik): compute from SSA use-def chain uses.
  IREE::HAL::MemoryTypeBitfield memoryTypes =
      IREE::HAL::MemoryTypeBitfield::DeviceLocal;
  IREE::HAL::BufferUsageBitfield bufferUsage =
      IREE::HAL::BufferUsageBitfield::Dispatch |
      IREE::HAL::BufferUsageBitfield::Transfer;

  // Compute the allocation size for the value.
  auto elementType = IREE::HAL::getElementTypeValue(
      streamValue.getType().cast<ShapedType>().getElementType());
  if (!elementType) {
    return {};
  }
  auto shape = IREE::HAL::getShapeDims(loc, streamValue, rewriter);
  if (!shape) {
    return {};
  }
  auto allocationSize = rewriter
                            .create<IREE::HAL::AllocatorComputeSizeOp>(
                                loc, allocator, *shape, elementType.getValue())
                            .getResult();

  auto buffer = rewriter
                    .create<IREE::HAL::AllocatorAllocateOp>(
                        loc, IREE::HAL::BufferType::get(rewriter.getContext()),
                        allocator, memoryTypes, bufferUsage, allocationSize)
                    .getResult();

  return buffer;
}

// Allocates transient buffers to store the intra-stream results and populates
// the |bufferSet| with the new mappings.
static void allocateTransientBuffers(IREE::Flow::ExStreamFragmentOp streamOp,
                                     BufferSet &bufferSet,
                                     ConversionPatternRewriter &rewriter) {
  LLVM_DEBUG(llvm::dbgs() << ": HAL allocateTransientBuffers: "
                          << *streamOp.getOperation() << "\n");

  // Allocate any needed transient buffers.
  for (auto &op : streamOp.body().front()) {
    if (auto subspanOp = dyn_cast<IREE::HAL::ConstantSubspanOp>(op)) {
      auto bufferValue = rewriter.createOrFold<IREE::HAL::VariableLoadOp>(
          subspanOp.getLoc(), IREE::HAL::BufferType::get(rewriter.getContext()),
          subspanOp.runtime_buffer().getLeafReference());
      auto offsetValue = rewriter.createOrFold<mlir::ConstantOp>(
          subspanOp.getLoc(), subspanOp.runtime_range().offsetAttr());
      auto lengthValue = rewriter.createOrFold<mlir::ConstantOp>(
          subspanOp.getLoc(), subspanOp.runtime_range().lengthAttr());
      auto subspanValue = rewriter.createOrFold<IREE::HAL::BufferSubspanOp>(
          subspanOp.getLoc(), bufferValue.getType(), bufferValue, offsetValue,
          lengthValue);
      bufferSet.rangeMap[subspanOp.result()] = BufferRange{subspanValue};
      continue;
    }

    auto tiedOp = dyn_cast<IREE::TiedOpInterface>(op);
    for (auto it : llvm::enumerate(op.getResults())) {
      auto result = it.value();
      if (!result.getType().isa<ShapedType>()) continue;

      // If the result is an output buffer we can just use that directly.
      if (bufferSet.rangeMap[result].buffer) {
        LLVM_DEBUG(llvm::dbgs() << "    -- SKIP ALREADY SET BUFFER RESULT("
                                << it.index() << "): " << op << "\n");
        continue;
      }

      // Tied results reuse their operand buffer.
      if (tiedOp) {
        auto tiedOperandIndex = tiedOp.getTiedResultOperandIndex(it.index());
        if (tiedOperandIndex.hasValue()) {
          LLVM_DEBUG(llvm::dbgs()
                     << "    -- REUSING TIED OPERAND("
                     << tiedOperandIndex.getValue() << ") BUFFER FOR RESULT("
                     << it.index() << "): " << op << "\n");
          auto operand = op.getOperand(tiedOperandIndex.getValue());
          auto operandBufferRange = bufferSet.rangeMap[operand];
          assert(operandBufferRange.buffer);
          bufferSet.rangeMap[result] = operandBufferRange;
          continue;
        }
      }

      LLVM_DEBUG(llvm::dbgs() << "    -- ALLOCATE BUFFER FOR RESULT("
                              << it.index() << "): " << op << "\n");
      auto buffer =
          allocateTransientBuffer(result, bufferSet.allocator, rewriter);
      auto bufferRange = BufferRange{buffer};
      bufferSet.rangeMap[result] = bufferRange;
      propagateTiedBuffer(bufferSet, result, bufferRange);
    }
  }
}

// Records a full execution barrier that forces visibility of all buffers.
static void recordFullExecutionBarrier(Value commandBuffer, Location loc,
                                       ConversionPatternRewriter &rewriter) {
  rewriter.create<IREE::HAL::CommandBufferExecutionBarrierOp>(
      loc, commandBuffer,
      IREE::HAL::ExecutionStageBitfield::CommandRetire |
          IREE::HAL::ExecutionStageBitfield::Dispatch,
      IREE::HAL::ExecutionStageBitfield::CommandIssue |
          IREE::HAL::ExecutionStageBitfield::Dispatch,
      IREE::HAL::ExecutionBarrierFlagBitfield::None);
}

static void recordInterfaceBindings(Value device, Value commandBuffer,
                                    IREE::Flow::DispatchOp &dispatchOp,
                                    IREE::HAL::InterfaceOp &interfaceOp,
                                    Value executableLayout,
                                    ArrayAttr bindingsAttr,
                                    BufferSet &bufferSet,
                                    ConversionPatternRewriter &rewriter) {
  // Accumulate a potentially sparse set of push constants.
  // If we had canonicalizers for hal.command_buffer.push_constants then we
  // would instead just emit each constant individually and let that collapse
  // things later on.
  int pushConstantBase = 0;  // always 0 today
  SmallVector<Value> pushConstantValues;
  pushConstantValues.resize(
      interfaceOp.push_constants().getValueOr(APInt(64, 0)).getSExtValue());

  // Accumulate a potentially sparse set of bindings.
  int setOrdinal = 0;  // always 0 today
  SmallVector<IREE::HAL::DescriptorSetBindingValue, 4> bindings;

  auto zeroOffset =
      rewriter.createOrFold<mlir::ConstantIndexOp>(dispatchOp.getLoc(), 0);
  auto push_buffer_binding = [&](StringRef bindingName, Value tensorValue) {
    auto bindingOp =
        interfaceOp.lookupSymbol<IREE::HAL::InterfaceBindingOp>(bindingName);
    assert(bindingOp);
    assert(bindingOp.set().getSExtValue() == 0);

    auto &bufferRange = bufferSet.rangeMap[tensorValue];
    assert(bufferRange.buffer && "buffer not preallocated");
    auto value = IREE::HAL::TensorRewriteAdaptor::getChecked(
        dispatchOp.getLoc(), tensorValue, bufferRange.buffer, rewriter);
    assert(value.hasValue() && "cannot create adaptor for tensor");
    auto byteLength = value->getByteLength();
    assert(byteLength);
    bindings.push_back(
        std::make_tuple(rewriter.createOrFold<mlir::ConstantOp>(
                            dispatchOp.getLoc(), bindingOp.bindingAttr()),
                        value->getBuffer(), zeroOffset, byteLength));
  };

  for (auto bindingAttr : bindingsAttr) {
    if (auto constantStorageAttr =
            bindingAttr.dyn_cast<IREE::HAL::ExConstantStorageAttr>()) {
      auto bindingOp = interfaceOp.lookupSymbol<IREE::HAL::InterfaceBindingOp>(
          constantStorageAttr.binding());
      assert(bindingOp);
      assert(bindingOp.set().getSExtValue() == setOrdinal);
      auto storageBuffer = rewriter.createOrFold<IREE::HAL::VariableLoadOp>(
          dispatchOp.getLoc(),
          IREE::HAL::BufferType::get(rewriter.getContext()),
          constantStorageAttr.storage());
      bindings.push_back(std::make_tuple(
          rewriter.createOrFold<mlir::ConstantOp>(dispatchOp.getLoc(),
                                                  bindingOp.bindingAttr()),
          storageBuffer,
          rewriter.createOrFold<mlir::ConstantOp>(
              dispatchOp.getLoc(), constantStorageAttr.offsetAttr()),
          rewriter.createOrFold<mlir::ConstantOp>(
              dispatchOp.getLoc(), constantStorageAttr.lengthAttr())));
    } else if (auto pushConstantAttr =
                   bindingAttr.dyn_cast<IREE::HAL::ExPushConstantAttr>()) {
      auto inputValue =
          dispatchOp.operands()[pushConstantAttr.operand().getSExtValue()];
      auto pushConstantValue = rewriter.getRemappedValue(inputValue);
      // Need an explicit index cast to i32 since the
      // CommandBufferPushConstantsOp is intrinsically i32 based.
      if (inputValue.getType().isa<IndexType>()) {
        pushConstantValue = rewriter.create<mlir::IndexCastOp>(
            dispatchOp.getLoc(), rewriter.getIntegerType(32),
            pushConstantValue);
      }
      pushConstantValues[pushConstantAttr.ordinal().getSExtValue()] =
          pushConstantValue;
    } else if (auto operandBufferAttr =
                   bindingAttr.dyn_cast<IREE::HAL::ExOperandBufferAttr>()) {
      auto tensorValue =
          dispatchOp.operands()[operandBufferAttr.operand().getSExtValue()];
      push_buffer_binding(operandBufferAttr.binding(), tensorValue);
    } else if (auto resultBufferAttr =
                   bindingAttr.dyn_cast<IREE::HAL::ExResultBufferAttr>()) {
      auto tensorValue =
          dispatchOp.results()[resultBufferAttr.result().getSExtValue()];
      push_buffer_binding(resultBufferAttr.binding(), tensorValue);
    }
  }

  rewriter.create<IREE::HAL::CommandBufferPushDescriptorSetOp>(
      dispatchOp.getLoc(), commandBuffer, executableLayout, setOrdinal,
      bindings);

  if (!pushConstantValues.empty()) {
    rewriter.create<IREE::HAL::CommandBufferPushConstantsOp>(
        dispatchOp.getLoc(), commandBuffer, executableLayout,
        rewriter.getIndexAttr(pushConstantBase), pushConstantValues);
  }
}

static void recordPushConstants(Value device, Value commandBuffer,
                                IREE::Flow::DispatchOp &dispatchOp,
                                IREE::HAL::InterfaceOp &interfaceOp,
                                Value executableLayout,
                                ConversionPatternRewriter &rewriter) {
  SmallVector<Value, 4> pushConstantValues;
  for (auto inputValue : dispatchOp.operands()) {
    if (inputValue.getType().isa<IndexType>() ||
        inputValue.getType().isa<IntegerType>()) {
      auto pushConstantValue = rewriter.getRemappedValue(inputValue);
      // Need an explicit index cast to i32 since the
      // CommandBufferPushConstantsOp is intrinsically i32 based.
      if (inputValue.getType().isa<IndexType>()) {
        pushConstantValue = rewriter.create<IndexCastOp>(
            dispatchOp.getLoc(), rewriter.getIntegerType(32),
            pushConstantValue);
      }
      pushConstantValues.push_back(pushConstantValue);
    }
  }
  if (pushConstantValues.empty()) {
    return;
  }

  uint64_t maxPushConstants =
      interfaceOp.push_constants().hasValue()
          ? interfaceOp.push_constants().getValue().getZExtValue()
          : 0;
  (void)maxPushConstants;
  assert(pushConstantValues.size() <= maxPushConstants &&
         "uniform buffer spilling not yet implemented");

  rewriter.create<IREE::HAL::CommandBufferPushConstantsOp>(
      dispatchOp.getLoc(), commandBuffer, executableLayout,
      rewriter.getIndexAttr(0), pushConstantValues);
}

static LogicalResult recordPushBindings(Value device, Value commandBuffer,
                                        IREE::Flow::DispatchOp &dispatchOp,
                                        Value executableLayout,
                                        BufferSet &bufferSet,
                                        ConversionPatternRewriter &rewriter) {
  LLVM_DEBUG(llvm::dbgs() << "HAL recordPushBindings: "
                          << *dispatchOp.getOperation() << "\n");
  uint32_t setOrdinal = 0;
  uint32_t bindingOrdinal = 0;
  SmallVector<IREE::HAL::DescriptorSetBindingValue, 4> bindings;
  auto zeroOffset =
      rewriter.createOrFold<mlir::ConstantIndexOp>(dispatchOp.getLoc(), 0);
  auto pushBinding =
      [&](Value tensorValue,
          IREE::HAL::MemoryAccessBitfield accessType) -> LogicalResult {
    auto &bufferRange = bufferSet.rangeMap[tensorValue];
    assert(bufferRange.buffer && "buffer not preallocated");
    auto value = IREE::HAL::TensorRewriteAdaptor::getChecked(
        dispatchOp.getLoc(), tensorValue, bufferRange.buffer, rewriter);
    if (!value.hasValue()) {
      return dispatchOp.emitOpError() << "cannot create adaptor for tensor";
    }
    auto byteLength = value->getByteLength();
    if (!byteLength) return failure();

    bindings.push_back(
        std::make_tuple(rewriter.createOrFold<mlir::ConstantIndexOp>(
                            dispatchOp.getLoc(), bindingOrdinal++),
                        value->getBuffer(), zeroOffset, byteLength));
    return success();
  };
  for (auto it : llvm::enumerate(dispatchOp.operands())) {
    LLVM_DEBUG(llvm::dbgs()
               << "  + OPERAND(" << it.index() << "): " << it.value() << "\n");
    if (it.value().getType().isa<TensorType>()) {
      if (failed(
              pushBinding(it.value(), IREE::HAL::MemoryAccessBitfield::Read))) {
        return failure();
      }
    }
  }
  for (auto it : llvm::enumerate(dispatchOp.results())) {
    LLVM_DEBUG(llvm::dbgs()
               << "  + RESULT(" << it.index() << "): " << it.value() << "\n");
    if (dispatchOp.getTiedResultOperandIndex(it.index())) {
      LLVM_DEBUG(llvm::dbgs() << "    TIED TO OPERAND; SKIP\n");
    } else {
      if (failed(pushBinding(it.value(),
                             IREE::HAL::MemoryAccessBitfield::DiscardWrite))) {
        return failure();
      }
    }
  }
  rewriter.create<IREE::HAL::CommandBufferPushDescriptorSetOp>(
      dispatchOp.getLoc(), commandBuffer, executableLayout, setOrdinal,
      bindings);
  return success();
}

// Records a dispatch operation.
static LogicalResult recordDispatch(Value device, Value commandBuffer,
                                    IREE::Flow::DispatchOp &dispatchOp,
                                    BufferSet &bufferSet,
                                    ConversionPatternRewriter &rewriter) {
  // Get the handle to the executable that is compatible with our device.
  auto executableOp =
      cast<IREE::HAL::ExecutableOp>(SymbolTable::lookupNearestSymbolFrom(
          dispatchOp, dispatchOp.executable()));

  IREE::HAL::TargetBackend::DispatchState dispatchState;
  dispatchState.dispatchOp = dispatchOp;
  dispatchState.executableOp = executableOp;
  dispatchState.device = device;
  dispatchState.commandBuffer = commandBuffer;
  for (auto dim : dispatchOp.workgroup_count()) {
    dispatchState.workgroupCount.push_back(rewriter.getRemappedValue(dim));
  }
  // TODO(benvanik): support extended push constants.
  dispatchState.basePushConstantOffset = 0;

  // Ask each target backend to record their dispatch logic.
  IREE::HAL::DeviceSwitchRewriter switchRewriter(dispatchOp.getLoc(),
                                                 /*resultTypes=*/TypeRange{},
                                                 device, rewriter);
  for (auto targetOp :
       executableOp.getBlock().getOps<IREE::HAL::ExecutableTargetOp>()) {
    for (auto &targetBackend : IREE::HAL::matchTargetBackends(
             {targetOp.target_backend_filter().str()})) {
      auto entryPointOps =
          targetOp.getBlock().getOps<IREE::HAL::ExecutableEntryPointOp>();
      if (entryPointOps.empty()) {
        return dispatchOp.emitOpError() << "need at least one entry point";
      }
      auto entryPointOp = *entryPointOps.begin();
      auto interfaceOp =
          dyn_cast<IREE::HAL::InterfaceOp>(SymbolTable::lookupSymbolIn(
              executableOp, entryPointOp.interfaceAttr()));
      auto executableLayout =
          rewriter.createOrFold<IREE::HAL::ExecutableLayoutLookupOp>(
              dispatchOp.getLoc(),
              IREE::HAL::ExecutableLayoutType::get(device.getContext()), device,
              interfaceOp.push_constantsAttr(),
              interfaceOp.getExecutableSetLayoutsAttr());

      // HACK: switch off for new-style bindings population if the metadata is
      // available.
      if (auto bindingsAttr =
              dispatchOp->getAttrOfType<ArrayAttr>("hal.bindings")) {
        recordInterfaceBindings(device, commandBuffer, dispatchOp, interfaceOp,
                                executableLayout, bindingsAttr, bufferSet,
                                rewriter);
      } else {
        recordPushConstants(device, commandBuffer, dispatchOp, interfaceOp,
                            executableLayout, rewriter);
        if (failed(recordPushBindings(device, commandBuffer, dispatchOp,
                                      executableLayout, bufferSet, rewriter))) {
          return failure();
        }
      }

      dispatchState.entryPointOp = entryPointOp;
      dispatchState.interfaceOp = interfaceOp;
      dispatchState.executableLayout = executableLayout;
      if (failed(targetBackend->recordDispatch(
              dispatchOp.getLoc(), dispatchState, switchRewriter))) {
        return dispatchOp.emitError()
               << "unable to record dispatch for target backend "
               << targetBackend->name();
      }
    }
  }
  switchRewriter.build();

  // Full barriers for now as we aren't scheduling things.
  // TODO(benvanik): don't add at the end of the command buffer (we could
  // also do a canonicalization step that removed trailing barriers).
  recordFullExecutionBarrier(commandBuffer, dispatchOp.getLoc(), rewriter);
  return success();
}

static LogicalResult recordTensorClone(Value device, Value commandBuffer,
                                       IREE::Flow::TensorCloneOp &cloneOp,
                                       BufferSet &bufferSet,
                                       ConversionPatternRewriter &rewriter) {
  auto &operandBuffer = bufferSet.rangeMap[cloneOp.operand()];
  auto &resultBuffer = bufferSet.rangeMap[cloneOp.result()];

  auto operand = IREE::HAL::TensorRewriteAdaptor::getChecked(
      cloneOp.getLoc(), cloneOp.operand(), operandBuffer.buffer, rewriter);
  auto result = IREE::HAL::TensorRewriteAdaptor::getChecked(
      cloneOp.getLoc(), cloneOp.result(), resultBuffer.buffer, rewriter);
  if (!operand.hasValue() || !result.hasValue()) {
    return cloneOp.emitOpError()
           << "cannot create adaptors for tensor clone operands/results";
  }

  auto zeroOffset =
      rewriter.createOrFold<mlir::ConstantIndexOp>(cloneOp.getLoc(), 0);
  auto byteLength = operand->getByteLength();
  if (!byteLength) return failure();

  rewriter.create<IREE::HAL::CommandBufferCopyBufferOp>(
      cloneOp.getLoc(), commandBuffer, operand->getBuffer(), zeroOffset,
      result->getBuffer(), zeroOffset, byteLength);

  // Full barriers for now as we aren't scheduling things.
  // TODO(benvanik): don't add at the end of the command buffer (we could
  // also do a canonicalization step that removed trailing barriers).
  recordFullExecutionBarrier(commandBuffer, cloneOp.getLoc(), rewriter);
  return success();
}

/// Convert a flow.tensor.slice to a copy.
// TODO(benvanik, ravishankarm): The copy is not necessary. If there are no
// other uses of this buffer you could just return the original buffer with an
// offset. This should either be handled here, or the `flow.tensor.slice` (or
// its equivalent `subtensor` op) must be pushed into dispatch regions (see
// Github issue #5131)
static LogicalResult recordTensorSlice(Value device, Value commandBuffer,
                                       IREE::Flow::TensorSliceOp &sliceOp,
                                       BufferSet &bufferSet,
                                       ConversionPatternRewriter &rewriter) {
  auto &sourceBuffer = bufferSet.rangeMap[sliceOp.source()];
  auto &resultBuffer = bufferSet.rangeMap[sliceOp.result()];

  // TODO(benvanik): use something other than the BufferRange::buffer?
  // This may require us to subview the buffer first.
  auto source = IREE::HAL::TensorRewriteAdaptor::getChecked(
      sliceOp.getLoc(), sliceOp.source(), sourceBuffer.buffer, rewriter);
  auto result = IREE::HAL::TensorRewriteAdaptor::getChecked(
      sliceOp.getLoc(), sliceOp.result(), resultBuffer.buffer, rewriter);
  if (!source.hasValue() || !result.hasValue()) {
    return sliceOp.emitOpError()
           << "cannot create adaptors for tensor slice operands/results";
  }

  auto zeroOffset =
      rewriter.createOrFold<mlir::ConstantIndexOp>(sliceOp.getLoc(), 0);

  // Compute the size of the update range.
  auto startIndices = llvm::to_vector<4>(llvm::map_range(
      sliceOp.start_indices(),
      [&](Value value) { return rewriter.getRemappedValue(value); }));
  auto shapeDims = result->getShapeDims();
  if (!shapeDims) return failure();
  auto sourceRange = source->computeRange(startIndices, *shapeDims);
  if (!sourceRange) return failure();

  // TODO(benvanik): slice left/mid/right, but really just don't do this.
  rewriter.create<IREE::HAL::CommandBufferCopyBufferOp>(
      sliceOp.getLoc(), commandBuffer, source->getBuffer(), sourceRange->offset,
      result->getBuffer(), zeroOffset, sourceRange->length);

  // Full barriers for now as we aren't scheduling things.
  // TODO(benvanik): don't add at the end of the command buffer (we could
  // also do a canonicalization step that removed trailing barriers).
  recordFullExecutionBarrier(commandBuffer, sliceOp.getLoc(), rewriter);
  return success();
}

static LogicalResult recordTensorUpdate(Value device, Value commandBuffer,
                                        IREE::Flow::TensorUpdateOp &updateOp,
                                        BufferSet &bufferSet,
                                        ConversionPatternRewriter &rewriter) {
  auto &updateBuffer = bufferSet.rangeMap[updateOp.update()];
  auto &targetBuffer = bufferSet.rangeMap[updateOp.target()];

  // TODO(benvanik): use something other than the BufferRange::buffer?
  // This may require us to subview the buffer first.
  auto update = IREE::HAL::TensorRewriteAdaptor::getChecked(
      updateOp.getLoc(), updateOp.update(), updateBuffer.buffer, rewriter);
  auto target = IREE::HAL::TensorRewriteAdaptor::getChecked(
      updateOp.getLoc(), updateOp.target(), targetBuffer.buffer, rewriter);
  if (!update.hasValue() || !target.hasValue()) {
    return updateOp.emitOpError()
           << "cannot create adaptors for tensor update operands/results";
  }

  auto zeroOffset =
      rewriter.createOrFold<mlir::ConstantIndexOp>(updateOp.getLoc(), 0);

  // Compute the size of the update range.
  auto startIndices = llvm::to_vector<4>(llvm::map_range(
      updateOp.start_indices(),
      [&](Value value) { return rewriter.getRemappedValue(value); }));
  auto shapeDims = update->getShapeDims();
  if (!shapeDims) return failure();
  auto targetRange =
      target->computeRange(startIndices, *update->getShapeDims());
  if (!targetRange) return failure();

  // TODO(benvanik): slice left/mid/right, but really just don't do this.
  rewriter.create<IREE::HAL::CommandBufferCopyBufferOp>(
      updateOp.getLoc(), commandBuffer, update->getBuffer(), zeroOffset,
      target->getBuffer(), targetRange->offset, targetRange->length);

  // Full barriers for now as we aren't scheduling things.
  // TODO(benvanik): don't add at the end of the command buffer (we could
  // also do a canonicalization step that removed trailing barriers).
  recordFullExecutionBarrier(commandBuffer, updateOp.getLoc(), rewriter);
  return success();
}

static LogicalResult recordStreamCommands(Value device, Value commandBuffer,
                                          Block &streamBlock,
                                          BufferSet &bufferSet,
                                          ConversionPatternRewriter &rewriter) {
  for (auto &op : streamBlock) {
    if (auto dispatchOp = dyn_cast<IREE::Flow::DispatchOp>(op)) {
      if (failed(recordDispatch(device, commandBuffer, dispatchOp, bufferSet,
                                rewriter))) {
        return failure();
      }
    } else if (auto cloneOp = dyn_cast<IREE::Flow::TensorCloneOp>(op)) {
      if (failed(recordTensorClone(device, commandBuffer, cloneOp, bufferSet,
                                   rewriter))) {
        return failure();
      }
    } else if (auto sliceOp = dyn_cast<IREE::Flow::TensorSliceOp>(op)) {
      if (failed(recordTensorSlice(device, commandBuffer, sliceOp, bufferSet,
                                   rewriter))) {
        return failure();
      }
    } else if (auto updateOp = dyn_cast<IREE::Flow::TensorUpdateOp>(op)) {
      if (failed(recordTensorUpdate(device, commandBuffer, updateOp, bufferSet,
                                    rewriter))) {
        return failure();
      }
    } else if (auto returnOp = dyn_cast<IREE::Flow::ReturnOp>(op)) {
      // No-op; handled by the buffer allocation.
    } else if (isa<ConstantOp>(op)) {
      // HACK: all this code is going away soon.
      auto newOp = rewriter.clone(op);
      op.replaceAllUsesWith(newOp);
    } else if (isa<IREE::HAL::ConstantSubspanOp>(op)) {
      // No work to perform.
    } else {
      return op.emitOpError() << "unexpected in stream";
    }
  }
  return success();
}

class ExStreamFragmentOpConversion
    : public OpConversionPattern<IREE::Flow::ExStreamFragmentOp> {
 public:
  using OpConversionPattern<
      IREE::Flow::ExStreamFragmentOp>::OpConversionPattern;
  LogicalResult matchAndRewrite(
      IREE::Flow::ExStreamFragmentOp streamOp, ArrayRef<Value> newOperands,
      ConversionPatternRewriter &rewriter) const override {
    IREE::Flow::ExStreamFragmentOp::Adaptor adaptor(
        newOperands, streamOp->getAttrDictionary());

    // TODO(benvanik): choose buffer mode/category based on stream commands.
    auto mode = IREE::HAL::CommandBufferModeBitfield::OneShot;
    auto category = IREE::HAL::CommandCategoryBitfield::Dispatch |
                    IREE::HAL::CommandCategoryBitfield::Transfer;

    // We'll use this buffer set to track the original and converted tensors
    // and buffers during conversion. Ideally we'd run some fancy allocation
    // analysis first to produce it.
    auto device =
        rewriter.createOrFold<IREE::HAL::ExSharedDeviceOp>(streamOp.getLoc());
    auto allocator =
        rewriter.create<IREE::HAL::DeviceAllocatorOp>(streamOp.getLoc(), device)
            .getResult();
    BufferSet bufferSet{allocator};

    // Remap non-tensor operands (such as workloads).
    auto &entryBlock = streamOp.body().front();
    for (int i = 0; i < adaptor.operands().size(); ++i) {
      if (streamOp.getOperand(i).getType().isa<TensorType>()) {
        bufferSet.rangeMap[entryBlock.getArgument(i)] =
            BufferRange{adaptor.operands()[i]};
      } else {
        rewriter.replaceUsesOfBlockArgument(entryBlock.getArgument(i),
                                            adaptor.operands()[i]);
      }
    }

    // Allocate buffers for outputs and transient buffers.
    allocateOutputBuffers(streamOp, bufferSet, rewriter);
    allocateTransientBuffers(streamOp, bufferSet, rewriter);

    // Allocate and begin the command buffer.
    // In a real version we would want to pick the device based on the placement
    // information attached to the stream.
    auto commandBuffer =
        rewriter.createOrFold<IREE::HAL::CommandBufferCreateOp>(
            streamOp.getLoc(),
            IREE::HAL::CommandBufferType::get(rewriter.getContext()), device,
            mode, category);
    rewriter.create<IREE::HAL::CommandBufferBeginOp>(streamOp.getLoc(),
                                                     commandBuffer);

    // Record all of the commands into the command buffer.
    if (failed(recordStreamCommands(device, commandBuffer, entryBlock,
                                    bufferSet, rewriter))) {
      return failure();
    }

    // End and submit the command buffer.
    // In a real version we'd want to setup a semaphore chain instead of
    // submitting and waiting.
    rewriter.create<IREE::HAL::CommandBufferEndOp>(streamOp.getLoc(),
                                                   commandBuffer);
    rewriter.create<IREE::HAL::ExSubmitAndWaitOp>(streamOp.getLoc(), device,
                                                  commandBuffer);

    // It's annoying, but we need to do this replacement at the very end as
    // otherwise we lose access to the original values (which we need for
    // shape information).
    for (int i = 0; i < adaptor.operands().size(); ++i) {
      if (adaptor.operands()[i].getType().isa<IREE::HAL::BufferType>()) {
        rewriter.replaceUsesOfBlockArgument(entryBlock.getArgument(i),
                                            adaptor.operands()[i]);
      }
    }

    rewriter.replaceOp(streamOp, bufferSet.outputBuffers);
    return success();
  }
};

}  // namespace

void populateFlowStreamToHALPatterns(MLIRContext *context,
                                     OwningRewritePatternList &patterns,
                                     TypeConverter &converter) {
  patterns.insert<ExStreamFragmentOpConversion>(context);
}

}  // namespace iree_compiler
}  // namespace mlir
