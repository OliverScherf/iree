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

#include "iree/hal/cuda/cuda_device.h"

#include "iree/base/status.h"
#include "iree/base/tracing.h"
#include "iree/hal/cuda/api.h"
#include "iree/hal/cuda/cuda_allocator.h"
#include "iree/hal/cuda/cuda_event.h"
#include "iree/hal/cuda/descriptor_set_layout.h"
#include "iree/hal/cuda/dynamic_symbols.h"
#include "iree/hal/cuda/event_semaphore.h"
#include "iree/hal/cuda/executable_layout.h"
#include "iree/hal/cuda/graph_command_buffer.h"
#include "iree/hal/cuda/nop_executable_cache.h"
#include "iree/hal/cuda/status_util.h"

//===----------------------------------------------------------------------===//
// iree_hal_cuda_device_t
//===----------------------------------------------------------------------===//

typedef struct {
  iree_hal_resource_t resource;
  iree_string_view_t identifier;

  // Optional driver that owns the CUDA symbols. We retain it for our lifetime
  // to ensure the symbols remains valid.
  iree_hal_driver_t* driver;

  CUdevice device;

  // TODO: support multiple streams.
  CUstream stream;
  iree_hal_cuda_context_wrapper_t context_wrapper;
  iree_hal_allocator_t* device_allocator;

} iree_hal_cuda_device_t;

extern const iree_hal_device_vtable_t iree_hal_cuda_device_vtable;

static iree_hal_cuda_device_t* iree_hal_cuda_device_cast(
    iree_hal_device_t* base_value) {
  IREE_HAL_ASSERT_TYPE(base_value, &iree_hal_cuda_device_vtable);
  return (iree_hal_cuda_device_t*)base_value;
}

static void iree_hal_cuda_device_destroy(iree_hal_device_t* base_device) {
  iree_hal_cuda_device_t* device = iree_hal_cuda_device_cast(base_device);
  iree_allocator_t host_allocator = iree_hal_device_host_allocator(base_device);
  IREE_TRACE_ZONE_BEGIN(z0);

  // There should be no more buffers live that use the allocator.
  iree_hal_allocator_release(device->device_allocator);
  CUDA_IGNORE_ERROR(device->context_wrapper.syms,
                    cuStreamDestroy(device->stream));

  // Finally, destroy the device.
  iree_hal_driver_release(device->driver);

  iree_allocator_free(host_allocator, device);

  IREE_TRACE_ZONE_END(z0);
}

static iree_status_t iree_hal_cuda_device_create_internal(
    iree_hal_driver_t* driver, iree_string_view_t identifier,
    CUdevice cu_device, CUstream stream, CUcontext context,
    iree_hal_cuda_dynamic_symbols_t* syms, iree_allocator_t host_allocator,
    iree_hal_device_t** out_device) {
  iree_hal_cuda_device_t* device = NULL;
  iree_host_size_t total_size = sizeof(*device) + identifier.size;
  IREE_RETURN_IF_ERROR(
      iree_allocator_malloc(host_allocator, total_size, (void**)&device));
  memset(device, 0, total_size);
  iree_hal_resource_initialize(&iree_hal_cuda_device_vtable, &device->resource);
  device->driver = driver;
  iree_hal_driver_retain(device->driver);
  uint8_t* buffer_ptr = (uint8_t*)device + sizeof(*device);
  buffer_ptr += iree_string_view_append_to_buffer(
      identifier, &device->identifier, (char*)buffer_ptr);
  device->device = cu_device;
  device->stream = stream;
  device->context_wrapper.cu_context = context;
  device->context_wrapper.host_allocator = host_allocator;
  device->context_wrapper.syms = syms;
  iree_status_t status = iree_hal_cuda_allocator_create(
      &device->context_wrapper, &device->device_allocator);
  if (iree_status_is_ok(status)) {
    *out_device = (iree_hal_device_t*)device;
  } else {
    iree_hal_device_release((iree_hal_device_t*)device);
  }
  return status;
}

iree_status_t iree_hal_cuda_device_create(iree_hal_driver_t* driver,
                                          iree_string_view_t identifier,
                                          iree_hal_cuda_dynamic_symbols_t* syms,
                                          CUdevice device,
                                          iree_allocator_t host_allocator,
                                          iree_hal_device_t** out_device) {
  IREE_TRACE_ZONE_BEGIN(z0);
  CUcontext context;
  IREE_RETURN_AND_END_ZONE_IF_ERROR(
      z0, CU_RESULT_TO_STATUS(syms, cuCtxCreate(&context, 0, device)));
  CUstream stream;
  iree_status_t status = CU_RESULT_TO_STATUS(
      syms, cuStreamCreate(&stream, CU_STREAM_NON_BLOCKING));

  if (iree_status_is_ok(status)) {
    status = iree_hal_cuda_device_create_internal(driver, identifier, device,
                                                  stream, context, syms,
                                                  host_allocator, out_device);
  }
  if (!iree_status_is_ok(status)) {
    if (stream) {
      syms->cuStreamDestroy(stream);
    }
    syms->cuCtxDestroy(context);
  }
  IREE_TRACE_ZONE_END(z0);
  return status;
}

static iree_string_view_t iree_hal_cuda_device_id(
    iree_hal_device_t* base_device) {
  iree_hal_cuda_device_t* device = iree_hal_cuda_device_cast(base_device);
  return device->identifier;
}

static iree_allocator_t iree_hal_cuda_device_host_allocator(
    iree_hal_device_t* base_device) {
  iree_hal_cuda_device_t* device = iree_hal_cuda_device_cast(base_device);
  return device->context_wrapper.host_allocator;
}

static iree_hal_allocator_t* iree_hal_cuda_device_allocator(
    iree_hal_device_t* base_device) {
  iree_hal_cuda_device_t* device = iree_hal_cuda_device_cast(base_device);
  return device->device_allocator;
}

static iree_status_t iree_hal_cuda_device_create_command_buffer(
    iree_hal_device_t* base_device, iree_hal_command_buffer_mode_t mode,
    iree_hal_command_category_t command_categories,
    iree_hal_queue_affinity_t queue_affinity,
    iree_hal_command_buffer_t** out_command_buffer) {
  iree_hal_cuda_device_t* device = iree_hal_cuda_device_cast(base_device);
  return iree_hal_cuda_graph_command_buffer_allocate(
      &device->context_wrapper, mode, command_categories, queue_affinity,
      out_command_buffer);
}

static iree_status_t iree_hal_cuda_device_create_descriptor_set(
    iree_hal_device_t* base_device,
    iree_hal_descriptor_set_layout_t* set_layout,
    iree_host_size_t binding_count,
    const iree_hal_descriptor_set_binding_t* bindings,
    iree_hal_descriptor_set_t** out_descriptor_set) {
  return iree_make_status(IREE_STATUS_UNIMPLEMENTED,
                          "non-push descriptor sets still need work");
}

static iree_status_t iree_hal_cuda_device_create_descriptor_set_layout(
    iree_hal_device_t* base_device,
    iree_hal_descriptor_set_layout_usage_type_t usage_type,
    iree_host_size_t binding_count,
    const iree_hal_descriptor_set_layout_binding_t* bindings,
    iree_hal_descriptor_set_layout_t** out_descriptor_set_layout) {
  iree_hal_cuda_device_t* device = iree_hal_cuda_device_cast(base_device);
  return iree_hal_cuda_descriptor_set_layout_create(
      &device->context_wrapper, usage_type, binding_count, bindings,
      out_descriptor_set_layout);
}

static iree_status_t iree_hal_cuda_device_create_event(
    iree_hal_device_t* base_device, iree_hal_event_t** out_event) {
  iree_hal_cuda_device_t* device = iree_hal_cuda_device_cast(base_device);
  return iree_hal_cuda_event_create(&device->context_wrapper, out_event);
}

static iree_status_t iree_hal_cuda_device_create_executable_cache(
    iree_hal_device_t* base_device, iree_string_view_t identifier,
    iree_hal_executable_cache_t** out_executable_cache) {
  iree_hal_cuda_device_t* device = iree_hal_cuda_device_cast(base_device);
  return iree_hal_cuda_nop_executable_cache_create(
      &device->context_wrapper, identifier, out_executable_cache);
}

static iree_status_t iree_hal_cuda_device_create_executable_layout(
    iree_hal_device_t* base_device, iree_host_size_t push_constants,
    iree_host_size_t set_layout_count,
    iree_hal_descriptor_set_layout_t** set_layouts,
    iree_hal_executable_layout_t** out_executable_layout) {
  iree_hal_cuda_device_t* device = iree_hal_cuda_device_cast(base_device);
  return iree_hal_cuda_executable_layout_create(
      &device->context_wrapper, set_layout_count, set_layouts, push_constants,
      out_executable_layout);
}

static iree_status_t iree_hal_cuda_device_create_semaphore(
    iree_hal_device_t* base_device, uint64_t initial_value,
    iree_hal_semaphore_t** out_semaphore) {
  iree_hal_cuda_device_t* device = iree_hal_cuda_device_cast(base_device);
  return iree_hal_cuda_semaphore_create(&device->context_wrapper, initial_value,
                                        out_semaphore);
}

static iree_status_t iree_hal_cuda_device_queue_submit(
    iree_hal_device_t* base_device,
    iree_hal_command_category_t command_categories,
    iree_hal_queue_affinity_t queue_affinity, iree_host_size_t batch_count,
    const iree_hal_submission_batch_t* batches) {
  iree_hal_cuda_device_t* device = iree_hal_cuda_device_cast(base_device);
  for (int i = 0; i < batch_count; i++) {
    for (int j = 0; j < batches[i].command_buffer_count; j++) {
      CUgraphExec exec = iree_hal_cuda_graph_command_buffer_exec(
          batches[i].command_buffers[j]);
      CUDA_RETURN_IF_ERROR(device->context_wrapper.syms,
                           cuGraphLaunch(exec, device->stream),
                           "cuGraphLaunch");
    }
  }
  // TODO(thomasraoux): Conservatively syncronize after every submit until we
  // support semaphores.
  CUDA_RETURN_IF_ERROR(device->context_wrapper.syms,
                       cuStreamSynchronize(device->stream),
                       "cuStreamSynchronize");
  return iree_ok_status();
}

static iree_status_t iree_hal_cuda_device_wait_semaphores_with_timeout(
    iree_hal_device_t* base_device, iree_hal_wait_mode_t wait_mode,
    const iree_hal_semaphore_list_t* semaphore_list,
    iree_duration_t timeout_ns) {
  return iree_make_status(IREE_STATUS_UNIMPLEMENTED,
                          "semaphore not implemented");
}

static iree_status_t iree_hal_cuda_device_wait_semaphores_with_deadline(
    iree_hal_device_t* base_device, iree_hal_wait_mode_t wait_mode,
    const iree_hal_semaphore_list_t* semaphore_list, iree_time_t deadline_ns) {
  return iree_make_status(IREE_STATUS_UNIMPLEMENTED,
                          "semaphore not implemented");
}

static iree_status_t iree_hal_cuda_device_wait_idle_with_deadline(
    iree_hal_device_t* base_device, iree_time_t deadline_ns) {
  iree_hal_cuda_device_t* device = iree_hal_cuda_device_cast(base_device);
  // Wait until the stream is done.
  // TODO(thomasraoux): CUDA doesn't support a deadline for wait, figure out how
  // to handle it better.
  CUDA_RETURN_IF_ERROR(device->context_wrapper.syms,
                       cuStreamSynchronize(device->stream),
                       "cuStreamSynchronize");
  return iree_ok_status();
}

static iree_status_t iree_hal_cuda_device_wait_idle_with_timeout(
    iree_hal_device_t* base_device, iree_duration_t timeout_ns) {
  return iree_hal_cuda_device_wait_idle_with_deadline(
      base_device, iree_relative_timeout_to_deadline_ns(timeout_ns));
}

const iree_hal_device_vtable_t iree_hal_cuda_device_vtable = {
    .destroy = iree_hal_cuda_device_destroy,
    .id = iree_hal_cuda_device_id,
    .host_allocator = iree_hal_cuda_device_host_allocator,
    .device_allocator = iree_hal_cuda_device_allocator,
    .create_command_buffer = iree_hal_cuda_device_create_command_buffer,
    .create_descriptor_set = iree_hal_cuda_device_create_descriptor_set,
    .create_descriptor_set_layout =
        iree_hal_cuda_device_create_descriptor_set_layout,
    .create_event = iree_hal_cuda_device_create_event,
    .create_executable_cache = iree_hal_cuda_device_create_executable_cache,
    .create_executable_layout = iree_hal_cuda_device_create_executable_layout,
    .create_semaphore = iree_hal_cuda_device_create_semaphore,
    .queue_submit = iree_hal_cuda_device_queue_submit,
    .wait_semaphores_with_deadline =
        iree_hal_cuda_device_wait_semaphores_with_deadline,
    .wait_semaphores_with_timeout =
        iree_hal_cuda_device_wait_semaphores_with_timeout,
    .wait_idle_with_deadline = iree_hal_cuda_device_wait_idle_with_deadline,
    .wait_idle_with_timeout = iree_hal_cuda_device_wait_idle_with_timeout,
};
