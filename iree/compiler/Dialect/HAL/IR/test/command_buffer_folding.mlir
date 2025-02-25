// RUN: iree-opt -split-input-file -canonicalize %s | iree-opt -split-input-file | IreeFileCheck %s

// CHECK-LABEL: @skip_command_buffer_device
func @skip_command_buffer_device() -> !hal.executable {
  // CHECK: %[[DEVICE:.+]] = hal.ex.shared_device
  %dev = hal.ex.shared_device : !hal.device
  %cmd = hal.command_buffer.create device(%dev : !hal.device)
                                     mode(OneShot)
                                     categories("Transfer|Dispatch") : !hal.command_buffer

  // CHECK-NOT: hal.command_buffer.device
  //      CHECK: = hal.executable.lookup device(%[[DEVICE]] : !hal.device)
  // CHECK-SAME:     executable(@executable_name) : !hal.executable
  %0 = hal.command_buffer.device<%cmd : !hal.command_buffer> : !hal.device
  %exe = hal.executable.lookup device(%dev : !hal.device)
                           executable(@executable_name) : !hal.executable

  return %exe : !hal.executable
}

// -----

// CHECK-LABEL: @fold_buffer_subspan_into_push_descriptor_set
//  CHECK-SAME: %[[CMD:.+]]: !hal.command_buffer,
//  CHECK-SAME: %[[LAYOUT:.+]]: !hal.executable_layout,
//  CHECK-SAME: %[[BASE_BUFFER:.+]]: !hal.buffer
func @fold_buffer_subspan_into_push_descriptor_set(
    %cmd: !hal.command_buffer,
    %layout: !hal.executable_layout,
    %buffer: !hal.buffer
  ) {
  %c0 = constant 0 : index
  %c1 = constant 1 : index
  %c2 = constant 2 : index
  %c4 = constant 4 : index
  %c4096 = constant 4096 : index
  %c8000 = constant 8000 : index
  %c262140 = constant 262140 : index
  %c262144 = constant 262144 : index
  %subspan = hal.buffer.subspan<%buffer : !hal.buffer>[%c4096, %c262144] : !hal.buffer
  //      CHECK: hal.command_buffer.push_descriptor_set
  // CHECK-SAME:   bindings([
  hal.command_buffer.push_descriptor_set<%cmd : !hal.command_buffer>
      layout(%layout : !hal.executable_layout)[%c0]
      bindings([
        // 0 + 4096:
        // CHECK-NEXT: %c0 = (%[[BASE_BUFFER]] : !hal.buffer)[%c4096, %c8000]
        %c0 = (%subspan : !hal.buffer)[%c0, %c8000],
        // 4096 + 4:
        // CHECK-NEXT: %c1 = (%[[BASE_BUFFER]] : !hal.buffer)[%c4100, %c262140]
        %c1 = (%subspan : !hal.buffer)[%c4, %c262140],
        // No change:
        // CHECK-NEXT: %c2 = (%[[BASE_BUFFER]] : !hal.buffer)[%c4096, %c262144]
        %c2 = (%buffer : !hal.buffer)[%c4096, %c262144]
      ])
  return
}
