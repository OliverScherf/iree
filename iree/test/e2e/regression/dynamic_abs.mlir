// RUN: iree-run-mlir -export-all -iree-hal-target-backends=vmla %s | IreeFileCheck %s
// RUN: [[ $IREE_LLVMAOT_DISABLE == 1 ]] || (iree-run-mlir -export-all -iree-hal-target-backends=dylib-llvm-aot %s | IreeFileCheck %s)
// RUN: [[ $IREE_LLVMAOT_DISABLE == 1 ]] || (iree-run-mlir -export-all -iree-hal-target-backends=dylib-llvm-aot -iree-flow-dispatch-linalg-on-tensors -iree-codegen-llvm-experimental-linalg-on-tensors %s | IreeFileCheck %s)

// CHECK-LABEL: EXEC @dynamic_tensor
func @dynamic_tensor() -> tensor<?x?xf32> attributes { iree.module.export } {
  %input = iree.dynamic_shape_constant dense<[[-1.0, 2.0, -3.0], [4.0, -5.0, 6.0]]> : tensor<2x3xf32> -> tensor<?x?xf32>
  %res = "mhlo.abs"(%input) : (tensor<?x?xf32>) -> tensor<?x?xf32>
  return %res : tensor<?x?xf32>
}

// CHECK: 2x3xf32=[1 2 3][4 5 6]
