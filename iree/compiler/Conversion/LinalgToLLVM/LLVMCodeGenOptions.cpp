// Copyright 2021 Google LLC
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

#include "iree/compiler/Conversion/LinalgToLLVM/LLVMCodeGenOptions.h"

#include "llvm/Support/CommandLine.h"

namespace mlir {
namespace iree_compiler {

static llvm::cl::opt<bool> clEnableLLVMLinalgOnTensors(
    "iree-codegen-llvm-experimental-linalg-on-tensors",
    llvm::cl::desc("Enable the linalg on tensors experimental LLVM path"),
    llvm::cl::init(false));

static llvm::cl::opt<bool> convImg2ColConversion(
    "iree-codegen-linalg-to-llvm-conv-img2col-conversion",
    llvm::cl::desc("Enable rewriting linalg.conv_2d_input_nhwc_filter_hwcf "
                   "linalg.generic that does img2col buffer packing + "
                   "linag.matmul"),
    llvm::cl::init(false));

static llvm::cl::opt<bool> unfusedFMA(
    "iree-codegen-linalg-to-llvm-use-unfused-fma",
    llvm::cl::desc("Enable rewriting llvm.fma to its unfused version."),
    llvm::cl::init(false));

LLVMCodegenOptions getLLVMCodegenOptionsFromClOptions() {
  LLVMCodegenOptions options;
  options.usingLinalgOnTensors = clEnableLLVMLinalgOnTensors;
  options.useConvImg2Col = convImg2ColConversion;
  options.unfuseFMAOps = unfusedFMA;
  return options;
}

}  // namespace iree_compiler
}  // namespace mlir
