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

#include "iree/compiler/Dialect/VMLA/IR/VMLATypes.h"

#include "llvm/ADT/StringExtras.h"

// Order matters:
#include "iree/compiler/Dialect/VMLA/IR/VMLAEnums.cpp.inc"

namespace mlir {
namespace iree_compiler {
namespace IREE {
namespace VMLA {

#include "iree/compiler/Dialect/VMLA/IR/VMLAOpInterface.cpp.inc"

}  // namespace VMLA
}  // namespace IREE
}  // namespace iree_compiler
}  // namespace mlir
