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

#include <numeric>

#include "iree/compiler/Dialect/Shape/IR/ShapeDialect.h"
#include "iree/compiler/Dialect/Shape/IR/ShapeOps.h"
#include "iree/compiler/Dialect/Shape/IR/ShapeTypes.h"
#include "iree/compiler/Dialect/Shape/Transforms/Passes.h"
#include "mlir-hlo/Dialect/mhlo/IR/hlo_ops.h"
#include "mlir/Dialect/StandardOps/IR/Ops.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/MLIRContext.h"
#include "mlir/IR/PatternMatch.h"
#include "mlir/Pass/PassRegistry.h"
#include "mlir/Transforms/DialectConversion.h"

namespace mlir {
namespace iree_compiler {
namespace Shape {
namespace {

class ConvertDynamicBroadcastInDim
    : public OpConversionPattern<mhlo::DynamicBroadcastInDimOp> {
  using OpConversionPattern::OpConversionPattern;
  LogicalResult matchAndRewrite(
      mhlo::DynamicBroadcastInDimOp op, ArrayRef<Value> operands,
      ConversionPatternRewriter &rewriter) const override {
    mhlo::DynamicBroadcastInDimOp::Adaptor adapter(operands);
    Value rankedShape = rewriter.create<Shape::FromExtentTensorOp>(
        op.getLoc(), adapter.output_dimensions());
    rewriter.replaceOpWithNewOp<Shape::RankedBroadcastInDimOp>(
        op, op.getType(), adapter.operand(), rankedShape,
        op.broadcast_dimensions());
    return success();
  }
};

class ConvertDynamicIota : public OpConversionPattern<mhlo::DynamicIotaOp> {
  using OpConversionPattern::OpConversionPattern;
  LogicalResult matchAndRewrite(
      mhlo::DynamicIotaOp op, ArrayRef<Value> operands,
      ConversionPatternRewriter &rewriter) const override {
    auto resultTy = op.getType().cast<ShapedType>();
    if (resultTy.getRank() != 1) {
      return failure();
    }

    auto rankedShape = rewriter.create<Shape::FromExtentTensorOp>(
        op.getLoc(), op.getOperand());
    rewriter.replaceOpWithNewOp<Shape::IotaOp>(op, op.getType(), rankedShape);
    return success();
  }
};

class ConvertHLOToShapePass
    : public PassWrapper<ConvertHLOToShapePass, FunctionPass> {
  void getDependentDialects(DialectRegistry &registry) const override {
    registry.insert<ShapeDialect>();
  }

  void runOnFunction() override {
    ConversionTarget conversionTarget(getContext());
    OwningRewritePatternList conversionPatterns(&getContext());

    conversionTarget.addLegalDialect<ShapeDialect>();
    conversionTarget.addLegalDialect<StandardOpsDialect>();
    conversionTarget.addLegalDialect<mhlo::MhloDialect>();

    conversionTarget.addIllegalOp<mhlo::DynamicBroadcastInDimOp>();
    conversionPatterns.insert<ConvertDynamicBroadcastInDim>(&getContext());

    conversionTarget.addIllegalOp<mhlo::DynamicIotaOp>();
    conversionPatterns.insert<ConvertDynamicIota>(&getContext());

    if (failed(applyPartialConversion(getFunction(), conversionTarget,
                                      std::move(conversionPatterns)))) {
      return signalPassFailure();
    }
  }
};

}  // namespace

// Converts shape-sensitive HLOs to be based on facilities in the shape
// dialect.
std::unique_ptr<OperationPass<FuncOp>> createConvertHLOToShapePass() {
  return std::make_unique<Shape::ConvertHLOToShapePass>();
}

static PassRegistration<Shape::ConvertHLOToShapePass> pass(
    "iree-shape-convert-hlo",
    "Converts dynamic shape dependent HLO ops to shaped variants.");

}  // namespace Shape
}  // namespace iree_compiler
}  // namespace mlir
