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

#ifndef IREE_VM_OPS_H_
#define IREE_VM_OPS_H_

#include <stdint.h>

#include "iree/base/api.h"

//===------------------------------------------------------------------===//
// Globals
//===------------------------------------------------------------------===//

static inline int32_t vm_global_load_i32(uint8_t* base, uint32_t byte_offset) {
  const int32_t* global_ptr = (const int32_t*)(base + byte_offset);
  return *global_ptr;
}

static inline void vm_global_store_i32(uint8_t* base, uint32_t byte_offset,
                                       int32_t value) {
  int32_t* global_ptr = (int32_t*)(base + byte_offset);
  *global_ptr = value;
}

//===------------------------------------------------------------------===//
// Conditional assignment
//===------------------------------------------------------------------===//

static inline int32_t vm_select_i32(int32_t condition, int32_t true_value,
                                    int32_t false_value) {
  return condition ? true_value : false_value;
}

//===------------------------------------------------------------------===//
// Native integer arithmetic
//===------------------------------------------------------------------===//

static inline int32_t vm_add_i32(int32_t lhs, int32_t rhs) { return lhs + rhs; }
static inline int32_t vm_sub_i32(int32_t lhs, int32_t rhs) { return lhs - rhs; }
static inline int32_t vm_mul_i32(int32_t lhs, int32_t rhs) { return lhs * rhs; }
static inline int32_t vm_div_i32s(int32_t lhs, int32_t rhs) {
  return lhs / rhs;
}
static inline int32_t vm_div_i32u(int32_t lhs, int32_t rhs) {
  return (int32_t)(((uint32_t)lhs) / ((uint32_t)rhs));
}
static inline int32_t vm_rem_i32s(int32_t lhs, int32_t rhs) {
  return lhs % rhs;
}
static inline int32_t vm_rem_i32u(int32_t lhs, int32_t rhs) {
  return (int32_t)(((uint32_t)lhs) % ((uint32_t)rhs));
}
static inline int32_t vm_not_i32(int32_t operand) {
  return (int32_t)(~((uint32_t)operand));
}
static inline int32_t vm_and_i32(int32_t lhs, int32_t rhs) { return lhs & rhs; }
static inline int32_t vm_or_i32(int32_t lhs, int32_t rhs) { return lhs | rhs; }
static inline int32_t vm_xor_i32(int32_t lhs, int32_t rhs) { return lhs ^ rhs; }

//===------------------------------------------------------------------===//
// Casting and type conversion/emulation
//===------------------------------------------------------------------===//

static inline int32_t vm_trunc_i32i8(int32_t operand) {
  return (uint8_t)((uint32_t)operand);
}
static inline int32_t vm_trunc_i32i16(int32_t operand) {
  return (uint16_t)((uint32_t)operand);
}
static inline int32_t vm_ext_i8i32s(int32_t operand) {
  return (int32_t)((int8_t)operand);
}
static inline int32_t vm_ext_i8i32u(int32_t operand) {
  return (uint32_t)((uint8_t)operand);
}
static inline int32_t vm_ext_i16i32s(int32_t operand) {
  return (int32_t)((int16_t)operand);
}
static inline int32_t vm_ext_i16i32u(int32_t operand) {
  return (uint32_t)((uint16_t)operand);
}

//===------------------------------------------------------------------===//
// Native bitwise shifts and rotates
//===------------------------------------------------------------------===//

static inline int32_t vm_shl_i32(int32_t operand, int8_t amount) {
  return (int32_t)(operand << amount);
}
static inline int32_t vm_shr_i32s(int32_t operand, int8_t amount) {
  return (int32_t)(operand >> amount);
}
static inline int32_t vm_shr_i32u(int32_t operand, int8_t amount) {
  return (int32_t)(((uint32_t)operand) >> amount);
}

//===------------------------------------------------------------------===//
// Comparison ops
//===------------------------------------------------------------------===//

static inline int32_t vm_cmp_eq_i32(int32_t lhs, int32_t rhs) {
  return (lhs == rhs) ? 1 : 0;
}
static inline int32_t vm_cmp_ne_i32(int32_t lhs, int32_t rhs) {
  return (lhs != rhs) ? 1 : 0;
}
static inline int32_t vm_cmp_lt_i32s(int32_t lhs, int32_t rhs) {
  return (lhs < rhs) ? 1 : 0;
}
static inline int32_t vm_cmp_lt_i32u(int32_t lhs, int32_t rhs) {
  return (((uint32_t)lhs) < ((uint32_t)rhs)) ? 1 : 0;
}
static inline int32_t vm_cmp_nz_i32(int32_t operand) {
  return (operand != 0) ? 1 : 0;
}

//===------------------------------------------------------------------===//
// Control flow ops
//===------------------------------------------------------------------===//

static inline iree_status_t vm_fail_or_ok(int32_t status_code,
                                          iree_string_view_t message) {
  if (status_code != 0) {
    return iree_status_allocate(IREE_STATUS_FAILED_PRECONDITION, "<vm>", 0,
                                message);
  }
  return iree_ok_status();
}

//===------------------------------------------------------------------===//
// ExtI64: Conditional assignment
//===------------------------------------------------------------------===//

static inline int64_t vm_select_i64(int32_t condition, int64_t true_value,
                                    int64_t false_value) {
  return condition ? true_value : false_value;
}

//===------------------------------------------------------------------===//
// ExtI64: Native integer arithmetic ops
//===------------------------------------------------------------------===//

static inline int64_t vm_add_i64(int64_t lhs, int64_t rhs) { return lhs + rhs; }
static inline int64_t vm_sub_i64(int64_t lhs, int64_t rhs) { return lhs - rhs; }
static inline int64_t vm_mul_i64(int64_t lhs, int64_t rhs) { return lhs * rhs; }
static inline int64_t vm_div_i64s(int64_t lhs, int64_t rhs) {
  return lhs / rhs;
}
static inline int64_t vm_div_i64u(int64_t lhs, int64_t rhs) {
  return (int64_t)(((uint64_t)lhs) / ((uint64_t)rhs));
}
static inline int64_t vm_rem_i64s(int64_t lhs, int64_t rhs) {
  return lhs % rhs;
}
static inline int64_t vm_rem_i64u(int64_t lhs, int64_t rhs) {
  return (int64_t)(((uint64_t)lhs) % ((uint64_t)rhs));
}
static inline int64_t vm_not_i64(int64_t operand) {
  return (int64_t)(~((uint64_t)operand));
}
static inline int64_t vm_and_i64(int64_t lhs, int64_t rhs) { return lhs & rhs; }
static inline int64_t vm_or_i64(int64_t lhs, int64_t rhs) { return lhs | rhs; }
static inline int64_t vm_xor_i64(int64_t lhs, int64_t rhs) { return lhs ^ rhs; }

//===------------------------------------------------------------------===//
// ExtI64: Casting and type conversion/emulation
//===------------------------------------------------------------------===//

static inline int32_t vm_trunc_i64i32(int64_t operand) {
  return (uint32_t)((uint64_t)operand);
}
static inline int64_t vm_ext_i32i64s(int32_t operand) {
  return (int64_t)((int32_t)operand);
}
static inline int64_t vm_ext_i32i64u(int32_t operand) {
  return (uint64_t)((uint32_t)operand);
}

//===------------------------------------------------------------------===//
// ExtI64: Native bitwise shifts and rotates
//===------------------------------------------------------------------===//

static inline int64_t vm_shl_i64(int64_t operand, int8_t amount) {
  return (int64_t)(operand << amount);
}
static inline int64_t vm_shr_i64s(int64_t operand, int8_t amount) {
  return (int64_t)(operand >> amount);
}
static inline int64_t vm_shr_i64u(int64_t operand, int8_t amount) {
  return (int64_t)(((uint64_t)operand) >> amount);
}

//===------------------------------------------------------------------===//
// ExtI64: Comparison ops
//===------------------------------------------------------------------===//

static inline int32_t vm_cmp_eq_i64(int64_t lhs, int64_t rhs) {
  return (lhs == rhs) ? 1 : 0;
}
static inline int32_t vm_cmp_ne_i64(int64_t lhs, int64_t rhs) {
  return (lhs != rhs) ? 1 : 0;
}
static inline int32_t vm_cmp_lt_i64s(int64_t lhs, int64_t rhs) {
  return (lhs < rhs) ? 1 : 0;
}
static inline int32_t vm_cmp_lt_i64u(int64_t lhs, int64_t rhs) {
  return (((uint64_t)lhs) < ((uint64_t)rhs)) ? 1 : 0;
}
static inline int32_t vm_cmp_nz_i64(int64_t operand) {
  return (operand != 0) ? 1 : 0;
}

#endif  // IREE_VM_OPS_H_
