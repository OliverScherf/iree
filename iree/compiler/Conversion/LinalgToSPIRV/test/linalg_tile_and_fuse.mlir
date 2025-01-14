// RUN: iree-opt -split-input-file -pass-pipeline='hal.executable(hal.executable.target(iree-codegen-spirv-linalg-tile-and-distribute,iree-spirv-tile-and-vectorize-in-one-workgroup))' -iree-spirv-enable-vectorization -canonicalize -cse %s | IreeFileCheck %s

// TODO(GH-4901): Convert these tests back to use dynamic shapes when linalg on tensors becomes default.
hal.executable @conv_no_padding attributes {sym_visibility = "private"} {
  hal.interface @legacy_io {
    hal.interface.binding @arg0, set=0, binding=0, type="StorageBuffer", access="Read"
    hal.interface.binding @arg1, set=0, binding=1, type="StorageBuffer", access="Read"
    hal.interface.binding @ret0, set=0, binding=2, type="StorageBuffer", access="Write|Discard"
  }
  hal.executable.target @vulkan, filter="dylib*" {
    hal.executable.entry_point @conv_no_padding attributes {
      interface = @legacy_io, ordinal = 0 : index,
      signature = (!flow.dispatch.tensor<readonly:3x4x6x14xf32>, !flow.dispatch.tensor<readonly:2x16x16x6xf32>,
        !flow.dispatch.tensor<writeonly:2x13x11x14xf32>) -> ()}
    module attributes {
      spv.target_env =
        #spv.target_env<#spv.vce<v1.3,
        [Shader], [SPV_KHR_storage_buffer_storage_class]>,
        {max_compute_workgroup_invocations = 128 : i32,
         max_compute_workgroup_size = dense<[128, 128, 64]> : vector<3xi32>}>} {
      func @conv_no_padding() {
        %0 = iree.placeholder for "interace buffer"
          {binding = @legacy_io::@arg0, operand_result_index = 0 : i32} : memref<3x4x6x14xf32>
        %1 = iree.placeholder for "interace buffer"
          {binding = @legacy_io::@arg1, operand_result_index = 1 : i32} : memref<2x15x14x6xf32>
        %2 = iree.placeholder for "interace buffer"
          {binding = @legacy_io::@ret0, operand_result_index = 2 : i32} : memref<2x13x11x14xf32>
        linalg.conv_2d_input_nhwc_filter_hwcf {dilations = dense<1> : vector<2xi64>, strides = dense<1> : vector<2xi64>}
           ins (%1, %0: memref<2x15x14x6xf32>, memref<3x4x6x14xf32>)
          outs (%2: memref<2x13x11x14xf32>)
        return
      }
      hal.interface @legacy_io attributes {sym_visibility = "private"} {
        hal.interface.binding @arg0, set=0, binding=0, type="StorageBuffer", access="Read"
        hal.interface.binding @arg1, set=0, binding=1, type="StorageBuffer", access="Read"
        hal.interface.binding @ret0, set=0, binding=2, type="StorageBuffer", access="Write|Discard"
      }
    }
  }
}
//   CHECK-DAG: #[[MAP0:.+]] = affine_map<()[s0] -> (s0 * 4)>
//   CHECK-DAG: #[[MAP2:.+]] = affine_map<()[s0] -> (s0 * 32)>
//       CHECK: hal.executable.entry_point @conv_no_padding
//   CHECK-DAG:   %[[C2:.+]] = constant 2
//   CHECK-DAG:   %[[C4:.+]] = constant 4
//   CHECK-DAG:   %[[C1:.+]] = constant 1
//       CHECK:   hal.return %[[C1]], %[[C4]], %[[C2]]
//       CHECK: func @conv_no_padding()
//  CHECK-SAME:   local_size = dense<[32, 4, 1]>
//   CHECK-DAG:   %[[ARG0:.+]] = iree.placeholder {{.*}} @legacy_io::@arg0
//   CHECK-DAG:   %[[ARG1:.+]] = iree.placeholder {{.*}} @legacy_io::@arg1
//   CHECK-DAG:   %[[RET0:.+]] = iree.placeholder {{.*}} @legacy_io::@ret0
//   CHECK-DAG:   %[[BIDX:.+]] = "gpu.block_id"() {dimension = "x"}
//   CHECK-DAG:   %[[BIDY:.+]] = "gpu.block_id"() {dimension = "y"}
//   CHECK-DAG:   %[[BIDZ:.+]] = "gpu.block_id"() {dimension = "z"}
//       CHECK:   %[[LBY:.+]] = affine.apply #[[MAP0]]()[%[[BIDY]]]
//       CHECK:   %[[LBX:.+]] = affine.apply #[[MAP2]]()[%[[BIDX]]]
//       CHECK:   %[[SV_ARG1:.+]] = memref.subview %[[ARG1]]
//  CHECK-SAME:     [%[[BIDZ]], %[[LBY]], %[[LBX]], 0]
//       CHECK:   %[[SV_RET0:.+]] = memref.subview %[[RET0]]
//  CHECK-SAME:     [%[[BIDZ]], %[[LBY]], %[[LBX]], 0]
//       CHECK:   linalg.conv_2d_input_nhwc_filter_hwcf
//  CHECK-SAME:     "workgroup"
//  CHECK-SAME:     ins(%[[SV_ARG1]], %[[ARG0]]
//  CHECK-SAME:    outs(%[[SV_RET0]]


// -----

// TODO(GH-4901): Convert these tests back to use dynamic shapes when linalg on tensors becomes default.
hal.executable @matmul attributes {sym_visibility = "private"} {
  hal.interface @legacy_io {
    hal.interface.binding @arg0, set=0, binding=0, type="StorageBuffer", access="Read"
    hal.interface.binding @arg1, set=0, binding=1, type="StorageBuffer", access="Read"
    hal.interface.binding @ret0, set=0, binding=2, type="StorageBuffer", access="Write|Discard"
  }
  hal.executable.target @vulkan, filter="dylib*" {
    hal.executable.entry_point @matmul attributes {
      interface = @legacy_io, ordinal = 0 : index,
      signature = (!flow.dispatch.tensor<readonly:25x50xf32>, !flow.dispatch.tensor<readonly:50x75xf32>,
        !flow.dispatch.tensor<writeonly:25x75xf32>) -> ()}
    module attributes {
      spv.target_env =
        #spv.target_env<#spv.vce<v1.3,
        [Shader], [SPV_KHR_storage_buffer_storage_class]>,
        {max_compute_workgroup_invocations = 128 : i32,
         max_compute_workgroup_size = dense<[128, 128, 64]> : vector<3xi32>}>} {
      func @matmul() {
        %0 = iree.placeholder for "interace buffer"
          {binding = @legacy_io::@arg0, operand_result_index = 0 : i32} : memref<25x50xf32>
        %1 = iree.placeholder for "interace buffer"
          {binding = @legacy_io::@arg1, operand_result_index = 1 : i32} : memref<50x75xf32>
        %2 = iree.placeholder for "interace buffer"
          {binding = @legacy_io::@ret0, operand_result_index = 2 : i32} : memref<25x75xf32>
        linalg.matmul ins(%0, %1 : memref<25x50xf32>, memref<50x75xf32>)
                     outs(%2 : memref<25x75xf32>)
        return
      }
      hal.interface @legacy_io attributes {sym_visibility = "private"} {
        hal.interface.binding @arg0, set=0, binding=0, type="StorageBuffer", access="Read"
        hal.interface.binding @arg1, set=0, binding=1, type="StorageBuffer", access="Read"
        hal.interface.binding @ret0, set=0, binding=2, type="StorageBuffer", access="Write|Discard"
      }
    }
  }
}
//   CHECK-DAG: #[[MAP0:.+]] = affine_map<()[s0] -> (s0 * 8)>
//   CHECK-DAG: #[[MAP3:.+]] = affine_map<()[s0] -> (s0 * 16)>
//       CHECK: hal.executable.entry_point @matmul
//   CHECK-DAG:   %[[C1:.+]] = constant 1
//   CHECK-DAG:   %[[C4:.+]] = constant 4
//   CHECK-DAG:   %[[C5:.+]] = constant 5
//       CHECK:   hal.return %[[C5]], %[[C4]], %[[C1]]
//       CHECK: func @matmul()
//  CHECK-SAME:   local_size = dense<[16, 8, 1]>
//   CHECK-DAG:   %[[ARG0:.+]] = iree.placeholder {{.*}} @legacy_io::@arg0
//   CHECK-DAG:   %[[ARG1:.+]] = iree.placeholder {{.*}} @legacy_io::@arg1
//   CHECK-DAG:   %[[RET0:.+]] = iree.placeholder {{.*}} @legacy_io::@ret0
//   CHECK-DAG:   %[[BIDX:.+]] = "gpu.block_id"() {dimension = "x"}
//   CHECK-DAG:   %[[BIDY:.+]] = "gpu.block_id"() {dimension = "y"}
//   CHECK-NOT:   scf.parallel
//   CHECK-NOT:   scf.for
//       CHECK:   %[[LBY:.+]] = affine.apply #[[MAP0]]()[%[[BIDY]]]
//       CHECK:   %[[SV_ARG0:.+]] = memref.subview %[[ARG0]][%[[LBY]], 0]
//       CHECK:   %[[LBX:.+]] = affine.apply #[[MAP3]]()[%[[BIDX]]]
//       CHECK:   %[[SV_ARG1:.+]] = memref.subview %[[ARG1]][0, %[[LBX]]]
//       CHECK:   %[[SV_RET0:.+]] = memref.subview %[[RET0]][%[[LBY]], %[[LBX]]]
//       CHECK:   linalg.matmul
//  CHECK-SAME:     "workgroup"
//  CHECK-SAME:     ins(%[[SV_ARG0]], %[[SV_ARG1]]
//  CHECK-SAME:       )
//  CHECK-SAME:     outs(%[[SV_RET0]]
//  CHECK-SAME:       )

// -----

// TODO(GH-4901): Convert these tests back to use dynamic shapes when linalg on tensors becomes default.
hal.executable @pooling_nhwc_max attributes {sym_visibility = "private"} {
  hal.interface @legacy_io {
    hal.interface.binding @arg0, set=0, binding=0, type="StorageBuffer", access="Read"
    hal.interface.binding @arg1, set=0, binding=1, type="StorageBuffer", access="Read"
    hal.interface.binding @ret0, set=0, binding=2, type="StorageBuffer", access="Write|Discard"
  }
  hal.executable.target @vulkan, filter="vulkan*" {
    hal.executable.entry_point @pooling_nhwc_max attributes {
      interface = @legacy_io, ordinal = 0 : index,
      signature = (!flow.dispatch.tensor<readonly:2x16x16x6xf32>, !flow.dispatch.tensor<readonly:1x3x4x2xf32>,
        !flow.dispatch.tensor<writeonly:2x14x13x5xf32>) -> ()}
    module attributes {
      spv.target_env =
        #spv.target_env<#spv.vce<v1.3,
        [Shader], [SPV_KHR_storage_buffer_storage_class]>,
        {max_compute_workgroup_invocations = 128 : i32,
         max_compute_workgroup_size = dense<[128, 128, 64]> : vector<3xi32>}>} {
      func @pooling_nhwc_max() {
        %0 = iree.placeholder for "interace buffer"
          {binding = @legacy_io::@arg0, operand_result_index = 0 : i32} : memref<2x16x16x6xf32>
        %1 = iree.placeholder for "interace buffer"
          {binding = @legacy_io::@arg1, operand_result_index = 1 : i32} : memref<3x4xf32>
        %2 = iree.placeholder for "interace buffer"
          {binding = @legacy_io::@ret0, operand_result_index = 2 : i32} : memref<2x14x13x6xf32>
        linalg.pooling_nhwc_max
          {dilations = dense<1> : vector<2xi64>, strides = dense<1> : vector<2xi64>}
          ins(%0, %1: memref<2x16x16x6xf32>, memref<3x4xf32>)
          outs(%2: memref<2x14x13x6xf32>)
        return
      }
      hal.interface @legacy_io attributes {sym_visibility = "private"} {
        hal.interface.binding @arg0, set=0, binding=0, type="StorageBuffer", access="Read"
        hal.interface.binding @arg1, set=0, binding=1, type="StorageBuffer", access="Read"
        hal.interface.binding @ret0, set=0, binding=2, type="StorageBuffer", access="Write|Discard"
      }
    }
  }
}

//   CHECK-DAG: #[[MAP0:.+]] = affine_map<()[s0] -> (s0 * 4)>
//   CHECK-DAG: #[[MAP2:.+]] = affine_map<()[s0] -> (s0 * 32)>
//       CHECK: hal.executable.entry_point @pooling_nhwc_max
//   CHECK-DAG:   %[[C1:.+]] = constant 1
//   CHECK-DAG:   %[[C4:.+]] = constant 4
//       CHECK:   hal.return %[[C1]], %[[C4]], %[[C1]]
//       CHECK: func @pooling_nhwc_max()
//  CHECK-SAME:   local_size = dense<[32, 4, 1]>
//   CHECK-DAG:   %[[ARG0:.+]] = iree.placeholder {{.*}} {binding = @legacy_io::@arg0
//   CHECK-DAG:   %[[ARG1:.+]] = iree.placeholder {{.*}} {binding = @legacy_io::@arg1
//   CHECK-DAG:   %[[RET0:.+]] = iree.placeholder {{.*}} {binding = @legacy_io::@ret0
//   CHECK-DAG:   %[[BIDX:.+]] = "gpu.block_id"() {dimension = "x"}
//   CHECK-DAG:   %[[BIDY:.+]] = "gpu.block_id"() {dimension = "y"}
//       CHECK:   %[[LBY:.+]] = affine.apply #[[MAP0]]()[%[[BIDY]]]
//       CHECK:   %[[LBX:.+]] = affine.apply #[[MAP2]]()[%[[BIDX]]]
//       CHECK:   %[[SV_ARG0:.+]] = memref.subview %[[ARG0]][0, %[[LBY]], %[[LBX]], 0]
//       CHECK:   %[[SV_RET0:.+]] = memref.subview %[[RET0]][0, %[[LBY]], %[[LBX]], 0]
//       CHECK:   linalg.pooling_nhwc_max
//  CHECK-SAME:     "workgroup"
//  CHECK-SAME:     ins(%[[SV_ARG0]], %[[ARG1]]
//  CHECK-SAME:    outs(%[[SV_RET0]]

// -----

// TODO(GH-4901): Convert these tests back to use dynamic shapes when linalg on tensors becomes default.
hal.executable @matmul_fusion attributes {sym_visibility = "private"} {
  hal.interface @legacy_io {
    hal.interface.binding @arg0, set=0, binding=0, type="StorageBuffer", access="Read"
    hal.interface.binding @arg1, set=0, binding=1, type="StorageBuffer", access="Read"
    hal.interface.binding @ret0, set=0, binding=2, type="StorageBuffer", access="Write|Discard"
  }
  hal.executable.target @vulkan, filter="dylib*" {
    hal.executable.entry_point @matmul_fusion attributes {
      interface = @legacy_io, ordinal = 0 : index,
      signature = (!flow.dispatch.tensor<readonly:25x50xf32>, !flow.dispatch.tensor<readonly:50x75xf32>,
        !flow.dispatch.tensor<writeonly:25x75xf32>) -> ()}
    module attributes {
      spv.target_env =
        #spv.target_env<#spv.vce<v1.3,
        [Shader], [SPV_KHR_storage_buffer_storage_class]>,
        {max_compute_workgroup_invocations = 128 : i32,
         max_compute_workgroup_size = dense<[128, 128, 64]> : vector<3xi32>}>} {
      func @matmul_fusion() {
        %0 = iree.placeholder for "interace buffer"
          {binding = @legacy_io::@arg0, operand_result_index = 0 : i32} : memref<25x50xf32>
        %1 = iree.placeholder for "interace buffer"
          {binding = @legacy_io::@arg1, operand_result_index = 1 : i32} : memref<50x75xf32>
        %2 = iree.placeholder for "interace buffer"
          {binding = @legacy_io::@ret0, operand_result_index = 2 : i32} : memref<25x75xf32>
        %cst = constant 0.000000e+00 : f32
        linalg.fill(%2, %cst) : memref<25x75xf32>, f32
        linalg.matmul ins(%0, %1 : memref<25x50xf32>, memref<50x75xf32>)
          outs(%2 : memref<25x75xf32>)
        return
      }
      hal.interface @legacy_io attributes {sym_visibility = "private"} {
        hal.interface.binding @arg0, set=0, binding=0, type="StorageBuffer", access="Read"
        hal.interface.binding @arg1, set=0, binding=1, type="StorageBuffer", access="Read"
        hal.interface.binding @ret0, set=0, binding=2, type="StorageBuffer", access="Write|Discard"
      }
    }
  }
}
//   CHECK-DAG: #[[MAP0:.+]] = affine_map<()[s0] -> (s0 * 8)>
//   CHECK-DAG: #[[MAP3:.+]] = affine_map<()[s0] -> (s0 * 16)>
//       CHECK: hal.executable.entry_point @matmul_fusion
//   CHECK-DAG:   %[[C1:.+]] = constant 1
//   CHECK-DAG:   %[[C4:.+]] = constant 4
//   CHECK-DAG:   %[[C5:.+]] = constant 5
//       CHECK:   hal.return %[[C5]], %[[C4]], %[[C1]]
//       CHECK: func @matmul_fusion()
//  CHECK-SAME:   local_size = dense<[16, 8, 1]>
//   CHECK-DAG:   %[[ARG0:.+]] = iree.placeholder {{.+}} @legacy_io::@arg0
//   CHECK-DAG:   %[[ARG1:.+]] = iree.placeholder {{.+}} @legacy_io::@arg1
//   CHECK-DAG:   %[[RET0:.+]] = iree.placeholder {{.*}} @legacy_io::@ret0
//   CHECK-DAG:   %[[BIDX:.+]] = "gpu.block_id"() {dimension = "x"}
//   CHECK-DAG:   %[[BIDY:.+]] = "gpu.block_id"() {dimension = "y"}
//   CHECK-NOT:   scf.parallel
//   CHECK-NOT:   scf.for
//       CHECK:   %[[LBY:.+]] = affine.apply #[[MAP0]]()[%[[BIDY]]]
//       CHECK:   %[[SV_ARG0:.+]] = memref.subview %[[ARG0]][%[[LBY]], 0]
//       CHECK:   %[[LBX:.+]] = affine.apply #[[MAP3]]()[%[[BIDX]]]
//       CHECK:   %[[SV_ARG1:.+]] = memref.subview %[[ARG1]][0, %[[LBX]]]
//       CHECK:   %[[SV_RET0:.+]] = memref.subview %[[RET0]][%[[LBY]], %[[LBX]]]
//       CHECK:   %[[SV_RET0_1:.+]] = memref.subview %[[RET0]][%[[LBY]], %[[LBX]]]
//       CHECK:   linalg.fill(%[[SV_RET0_1]], %{{.+}})
//  CHECK-SAME:     "workgroup"
//       CHECK:   linalg.matmul
//  CHECK-SAME:     "workgroup"
//  CHECK-SAME:     ins(%[[SV_ARG0]], %[[SV_ARG1]]
//  CHECK-SAME:     outs(%[[SV_RET0]]

// -----

// TODO(GH-4901): Convert these tests back to use dynamic shapes when linalg on tensors becomes default.
hal.executable @conv_no_padding_fusion attributes {sym_visibility = "private"} {
  hal.interface @legacy_io {
    hal.interface.binding @arg0, set=0, binding=0, type="StorageBuffer", access="Read"
    hal.interface.binding @arg1, set=0, binding=1, type="StorageBuffer", access="Read"
    hal.interface.binding @ret0, set=0, binding=2, type="StorageBuffer", access="Write|Discard"
  }
  hal.executable.target @vulkan, filter="dylib*" {
    hal.executable.entry_point @conv_no_padding_fusion attributes {
      interface = @legacy_io, ordinal = 0 : index,
      signature = (!flow.dispatch.tensor<readonly:3x4x6x14xf32>, !flow.dispatch.tensor<readonly:2x15x14x6xf32>,
        !flow.dispatch.tensor<writeonly:2x13x11x14xf32>) -> ()}
    module attributes {
      spv.target_env =
        #spv.target_env<#spv.vce<v1.3,
        [Shader], [SPV_KHR_storage_buffer_storage_class]>,
        {max_compute_workgroup_invocations = 128 : i32,
         max_compute_workgroup_size = dense<[128, 128, 64]> : vector<3xi32>}>} {
      func @conv_no_padding_fusion() {
        %0 = iree.placeholder for "interace buffer"
          {binding = @legacy_io::@arg0, operand_result_index = 0 : i32} : memref<3x4x6x14xf32>
        %1 = iree.placeholder for "interace buffer"
          {binding = @legacy_io::@arg1, operand_result_index = 1 : i32} : memref<2x15x14x6xf32>
        %2 = iree.placeholder for "interace buffer"
          {binding = @legacy_io::@ret0, operand_result_index = 2 : i32} : memref<2x13x11x14xf32>
        %cst = constant 0.000000e+00 : f32
        linalg.fill(%2, %cst) : memref<2x13x11x14xf32>, f32
        linalg.conv_2d_input_nhwc_filter_hwcf {dilations = dense<1> : vector<2xi64>, strides = dense<1> : vector<2xi64>}
           ins (%1, %0: memref<2x15x14x6xf32>, memref<3x4x6x14xf32>)
          outs (%2: memref<2x13x11x14xf32>)
        return
      }
      hal.interface @legacy_io attributes {sym_visibility = "private"} {
        hal.interface.binding @arg0, set=0, binding=0, type="StorageBuffer", access="Read"
        hal.interface.binding @arg1, set=0, binding=1, type="StorageBuffer", access="Read"
        hal.interface.binding @ret0, set=0, binding=2, type="StorageBuffer", access="Write|Discard"
      }
    }
  }
}
//   CHECK-DAG: #[[MAP0:.+]] = affine_map<()[s0] -> (s0 * 4)>
//   CHECK-DAG: #[[MAP1:.+]] = affine_map<()[s0] -> (s0 * 32)>
//       CHECK: hal.executable.entry_point @conv_no_padding_fusion
//   CHECK-DAG:   %[[C2:.+]] = constant 2
//   CHECK-DAG:   %[[C4:.+]] = constant 4
//   CHECK-DAG:   %[[C1:.+]] = constant 1
//       CHECK:   hal.return %[[C1]], %[[C4]], %[[C2]]
//       CHECK: func @conv_no_padding_fusion()
//  CHECK-SAME:   local_size = dense<[32, 4, 1]>
//   CHECK-DAG:   %[[ARG0:.+]] = iree.placeholder {{.*}} @legacy_io::@arg0
//   CHECK-DAG:   %[[ARG1:.+]] = iree.placeholder {{.*}} @legacy_io::@arg1
//   CHECK-DAG:   %[[RET0:.+]] = iree.placeholder {{.*}} @legacy_io::@ret0
//   CHECK-DAG:   %[[BIDX:.+]] = "gpu.block_id"() {dimension = "x"}
//   CHECK-DAG:   %[[BIDY:.+]] = "gpu.block_id"() {dimension = "y"}
//   CHECK-DAG:   %[[BIDZ:.+]] = "gpu.block_id"() {dimension = "z"}
//       CHECK:   %[[LBY:.+]] = affine.apply #[[MAP0]]()[%[[BIDY]]]
//       CHECK:   %[[LBX:.+]] = affine.apply #[[MAP1]]()[%[[BIDX]]]
//       CHECK:   %[[SV_ARG1:.+]] = memref.subview %[[ARG1]]
//  CHECK-SAME:     [%[[BIDZ]], %[[LBY]], %[[LBX]], 0]
//       CHECK:   %[[SV_RET0:.+]] = memref.subview %[[RET0]]
//  CHECK-SAME:     [%[[BIDZ]], %[[LBY]], %[[LBX]], 0]
//       CHECK:   %[[SV_RET0_1:.+]] = memref.subview %[[RET0]]
//  CHECK-SAME:     [%[[BIDZ]], %[[LBY]], %[[LBX]], 0]
//       CHECK:   linalg.fill(%[[SV_RET0_1]], %{{.*}})
//  CHECK-SAME:     "workgroup"
//       CHECK:   linalg.conv_2d_input_nhwc_filter_hwcf
//  CHECK-SAME:     "workgroup"
//  CHECK-SAME:     ins(%[[SV_ARG1]], %[[ARG0]]
//  CHECK-SAME:    outs(%[[SV_RET0]]

// -----

// TODO(GH-4901): Convert these tests back to use dynamic shapes when linalg on tensors becomes default.
hal.executable @three_op_fusion attributes {sym_visibility = "private"} {
  hal.interface @legacy_io {
    hal.interface.binding @arg0, set=0, binding=0, type="StorageBuffer", access="Read"
    hal.interface.binding @arg1, set=0, binding=1, type="StorageBuffer", access="Read"
    hal.interface.binding @ret0, set=0, binding=2, type="StorageBuffer", access="Write|Discard"
  }
  hal.executable.target @vulkan, filter="dylib*" {
    hal.executable.entry_point @three_op_fusion attributes {
      interface = @legacy_io, ordinal = 0 : index,
      signature = (!flow.dispatch.tensor<readonly:25x50xf32>, !flow.dispatch.tensor<readonly:50x75xf32>,
        !flow.dispatch.tensor<writeonly:25x75xf32>) -> ()}
    module attributes {
      spv.target_env =
        #spv.target_env<#spv.vce<v1.3,
        [Shader], [SPV_KHR_storage_buffer_storage_class]>,
        {max_compute_workgroup_invocations = 128 : i32,
         max_compute_workgroup_size = dense<[128, 128, 64]> : vector<3xi32>}>} {
      func @three_op_fusion() {
        %cst = constant 0.000000e+00 : f32
        %c0 = constant 0 : index
        %c1 = constant 1 : index
        %0 = iree.placeholder for "interface buffer"
          {binding = @legacy_io::@arg0, operand_result_index = 0 : i32}
          : memref<25x50xf32>
        %1 = iree.placeholder for "interface buffer"
          {binding = @legacy_io::@arg1, operand_result_index = 1 : i32}
          : memref<50x75xf32>
        %2 = memref.alloc() : memref<25x75xf32>
        %3 =  iree.placeholder for "interface buffer"
          {binding = @legacy_io::@arg2, operand_result_index = 2 : i32}
          : memref<75xf32>
        %4 =  iree.placeholder for "interface buffer"
          {binding = @legacy_io::@ret0, operand_result_index = 3 : i32}
          : memref<25x75xf32>
        linalg.fill(%2, %cst) : memref<25x75xf32>, f32
        linalg.matmul ins(%0, %1 : memref<25x50xf32>, memref<50x75xf32>)
          outs(%2 : memref<25x75xf32>)
        linalg.generic
          {indexing_maps = [affine_map<(d0, d1) -> (d0, d1)>,
                            affine_map<(d0, d1) -> (d1)>,
                            affine_map<(d0, d1) -> (d0, d1)>],
           iterator_types = ["parallel", "parallel"]}
          ins(%2, %3 : memref<25x75xf32>, memref<75xf32>)
          outs(%4 : memref<25x75xf32>) {
          ^bb0(%arg0 : f32, %arg1 : f32, %arg2 : f32) :
            %5 = addf %arg0, %arg1 : f32
            linalg.yield %5 : f32
          }
        return
      }
      hal.interface @legacy_io attributes {sym_visibility = "private"} {
        hal.interface.binding @arg0, set=0, binding=0, type="StorageBuffer", access="Read"
        hal.interface.binding @arg1, set=0, binding=1, type="StorageBuffer", access="Read"
        hal.interface.binding @arg2, set=0, binding=2, type="StorageBuffer", access="Read"
        hal.interface.binding @ret0, set=0, binding=3, type="StorageBuffer", access="Write|Discard"
      }
    }
  }
}

//   CHECK-DAG: #[[MAP0:.+]] = affine_map<()[s0] -> (s0 * 8)>
//   CHECK-DAG: #[[MAP1:.+]] = affine_map<()[s0] -> (8, s0 * -8 + 25)>
//   CHECK-DAG: #[[MAP2:.+]] = affine_map<()[s0] -> (s0 * 16)>
//   CHECK-DAG: #[[MAP3:.+]] = affine_map<()[s0] -> (16, s0 * -16 + 75)>
//   CHECK-DAG: #[[MAP1_2:.+]] = affine_map<()[s0] -> (s0 * -8 + 25, 8)>
//   CHECK-DAG: #[[MAP3_2:.+]] = affine_map<()[s0] -> (s0 * -16 + 75, 16)>
//       CHECK: hal.executable.entry_point @three_op_fusion
//   CHECK-DAG:   %[[C1:.+]] = constant 1
//   CHECK-DAG:   %[[C4:.+]] = constant 4
//   CHECK-DAG:   %[[C5:.+]] = constant 5
//       CHECK:   hal.return %[[C5]], %[[C4]], %[[C1]]
//       CHECK: func @three_op_fusion
//   CHECK-DAG:   %[[ALLOC:.+]] = memref.alloc() : memref<8x16xf32, 3>
//   CHECK-DAG:   %[[ARG0:.+]] = iree.placeholder {{.*}} @legacy_io::@arg0
//   CHECK-DAG:   %[[ARG1:.+]] = iree.placeholder {{.*}} @legacy_io::@arg1
//   CHECK-DAG:   %[[ARG2:.+]] = iree.placeholder {{.*}} @legacy_io::@arg2
//   CHECK-DAG:   %[[RET0:.+]] = iree.placeholder {{.*}} @legacy_io::@ret0
//   CHECK-DAG:   %[[BIDX:.+]] = "gpu.block_id"() {dimension = "x"}
//   CHECK-DAG:   %[[BIDY:.+]] = "gpu.block_id"() {dimension = "y"}
//   CHECK-NOT:   scf.parallel
//   CHECK-NOT:   scf.for
//       CHECK:   %[[LBX:.+]] = affine.apply #[[MAP2]]()[%[[BIDX]]]
//       CHECK:   %[[TILE_N_2:.+]] = affine.min #[[MAP3]]()[%[[BIDX]]]
//       CHECK:   %[[SV_ARG2:.+]] = memref.subview %[[ARG2]][%[[LBX]]] [%[[TILE_N_2]]]
//       CHECK:   %[[LBY:.+]] = affine.apply #[[MAP0]]()[%[[BIDY]]]
//       CHECK:   %[[TILE_M_2:.+]] = affine.min #[[MAP1]]()[%[[BIDY]]]
//       CHECK:   %[[SV_RET0:.+]] = memref.subview %[[RET0]][%[[LBY]], %[[LBX]]]
//  CHECK-SAME:     [%[[TILE_M_2]], %[[TILE_N_2]]]
//       CHECK:   %[[TILE_M:.+]] = affine.min #[[MAP1_2]]()[%[[BIDY]]]
//       CHECK:   %[[SV_ARG0:.+]] = memref.subview %[[ARG0]][%[[LBY]], 0] [%[[TILE_M]], 50]
//       CHECK:   %[[TILE_N:.+]] = affine.min #[[MAP3_2]]()[%[[BIDX]]]
//       CHECK:   %[[SV_ARG1:.+]] = memref.subview %[[ARG1]][0, %[[LBX]]] [50, %[[TILE_N]]]
//       CHECK:   %[[SV_ALLOC:.+]] = memref.subview %[[ALLOC]][0, 0] [%[[TILE_M]], %[[TILE_N]]]
//       CHECK:   linalg.fill(%[[SV_ALLOC]], %{{.+}})
//  CHECK-SAME:     "workgroup"
//       CHECK:   linalg.matmul
//  CHECK-SAME:     "workgroup"
//  CHECK-SAME:     ins(%[[SV_ARG0]], %[[SV_ARG1]]
//  CHECK-SAME:       )
//  CHECK-SAME:     outs(%[[SV_ALLOC]]
//  CHECK-SAME:       )
//       CHECK:   linalg.generic
//  CHECK-SAME:     ins(%[[SV_ALLOC]], %[[SV_ARG2]]
//  CHECK-SAME:       )
//  CHECK-SAME:     outs(%[[SV_RET0]]
//  CHECK-SAME:       )
