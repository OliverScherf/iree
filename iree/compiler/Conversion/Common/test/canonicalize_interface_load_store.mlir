// RUN: iree-opt -split-input-file -iree-codegen-cleanup-buffer-alloc-view %s | IreeFileCheck %s

// CHECK-LABEL: func @fold_reshape()
func @fold_reshape() {
  // CHECK: %[[C0:.+]] = constant 0 : index
  %c0 = constant 0 : index
  %c1 = constant 1 : index
  // CHECK: %[[ARG:.+]] = hal.interface.binding.subspan @interface_io::@arg0[%[[C0]]] : !flow.dispatch.tensor<readonly:3x3x96xf32>
  %1 = hal.interface.binding.subspan @interface_io::@arg0[%c0] : !flow.dispatch.tensor<readonly:3x3x1x96xf32>
  %2 = hal.interface.binding.subspan @interface_io::@ret0[%c0] : !flow.dispatch.tensor<writeonly:3x3x96xf32>
  // CHECK: %[[LOAD:.+]] = flow.dispatch.tensor.load %[[ARG]], {{.*}} : !flow.dispatch.tensor<readonly:3x3x96xf32> -> tensor<3x3x96xf32>
  %3 = flow.dispatch.tensor.load %1, offsets=[], sizes =[], strides=[] : !flow.dispatch.tensor<readonly:3x3x1x96xf32> -> tensor<3x3x1x96xf32>
  %4 = linalg.tensor_reshape %3 [affine_map<(d0, d1, d2, d3) -> (d0, d1, d2, d3)>] : tensor<3x3x1x96xf32> into tensor<864xf32>
  %5 = linalg.tensor_reshape %4 [affine_map<(d0, d1, d2) -> (d0, d1, d2)>] : tensor<864xf32> into tensor<3x3x96xf32>
  //  CHECK: flow.dispatch.tensor.store %[[LOAD]], {{.*}}
  flow.dispatch.tensor.store %5, %2, offsets = [%c0, %c0, %c0], sizes = [%c1, %c1, %c1], strides = [%c1, %c1, %c1] : tensor<3x3x96xf32> -> !flow.dispatch.tensor<writeonly:3x3x96xf32>
  return
}

hal.interface @interface_io attributes {sym_visibility = "private"} {
  hal.interface.binding @arg0, set=0, binding=0, type="StorageBuffer", access="Read"
  hal.interface.binding @ret0, set=0, binding=0, type="StorageBuffer", access="Write|Discard"
}


// -----

// CHECK-LABEL: func @dont_fold_reshape_with_not_full_load()
func @dont_fold_reshape_with_not_full_load() {
  %c0 = constant 0 : index
  %c1 = constant 1 : index
  %c3 = constant 3 : index
  %c96 = constant 96 : index
  %1 = hal.interface.binding.subspan @interface_io::@arg0[%c0] : !flow.dispatch.tensor<readonly:6x3x1x96xf32>
  %2 = hal.interface.binding.subspan @interface_io::@ret0[%c0] : !flow.dispatch.tensor<writeonly:3x3x96xf32>
  %3 = flow.dispatch.tensor.load %1, offsets = [%c3, %c0, %c0, %c0], sizes = [%c3, %c3, %c1, %c96], strides = [%c1, %c1, %c1, %c1] : !flow.dispatch.tensor<readonly:6x3x1x96xf32> -> tensor<3x3x1x96xf32>
  // CHECK-COUNT-2: linalg.tensor_reshape
  %4 = linalg.tensor_reshape %3 [affine_map<(d0, d1, d2, d3) -> (d0, d1, d2, d3)>] : tensor<3x3x1x96xf32> into tensor<864xf32>
  %5 = linalg.tensor_reshape %4 [affine_map<(d0, d1, d2) -> (d0, d1, d2)>] : tensor<864xf32> into tensor<3x3x96xf32>
  flow.dispatch.tensor.store %5, %2, offsets = [%c0, %c0, %c0], sizes = [%c1, %c1, %c1], strides = [%c1, %c1, %c1] : tensor<3x3x96xf32> -> !flow.dispatch.tensor<writeonly:3x3x96xf32>
  return
}

hal.interface @interface_io attributes {sym_visibility = "private"} {
  hal.interface.binding @arg0, set=0, binding=0, type="StorageBuffer", access="Read"
  hal.interface.binding @ret0, set=0, binding=0, type="StorageBuffer", access="Write|Discard"
}
