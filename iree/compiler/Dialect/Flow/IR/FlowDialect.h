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

#ifndef IREE_COMPILER_DIALECT_FLOW_IR_FLOWDIALECT_H_
#define IREE_COMPILER_DIALECT_FLOW_IR_FLOWDIALECT_H_

#include "mlir/IR/Dialect.h"
#include "mlir/IR/OpDefinition.h"
#include "mlir/IR/SymbolTable.h"

namespace mlir {
namespace iree_compiler {
namespace IREE {
namespace Flow {

#include "iree/compiler/Dialect/Flow/IR/FlowInterfaces.h.inc"

class FlowDialect : public Dialect {
 public:
  explicit FlowDialect(MLIRContext *context);
  static StringRef getDialectNamespace() { return "flow"; }

  Operation *materializeConstant(OpBuilder &builder, Attribute value, Type type,
                                 Location loc) override;

  Type parseType(DialectAsmParser &parser) const override;
  void printType(Type type, DialectAsmPrinter &p) const override;

  static bool isDialectOp(Operation *op) {
    return op && op->getDialect() &&
           op->getDialect()->getNamespace() == getDialectNamespace();
  }
};

}  // namespace Flow
}  // namespace IREE
}  // namespace iree_compiler
}  // namespace mlir

#endif  // IREE_COMPILER_DIALECT_FLOW_IR_FLOWDIALECT_H_
