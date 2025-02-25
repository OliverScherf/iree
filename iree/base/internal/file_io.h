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

#ifndef IREE_BASE_INTERNAL_FILE_IO_H_
#define IREE_BASE_INTERNAL_FILE_IO_H_

#include <string>

#include "iree/base/api.h"

namespace iree {
namespace file_io {

// Checks if a file exists at the provided path.
//
// Returns an OK status if the file definitely exists.
// Errors can include PermissionDeniedError, NotFoundError, etc.
iree_status_t FileExists(const char* path);

// Synchronously reads a file's contents into a string.
iree_status_t GetFileContents(const char* path, std::string* out_contents);

// Synchronously writes a string into a file, overwriting its contents.
iree_status_t SetFileContents(const char* path, iree_const_byte_span_t content);

}  // namespace file_io
}  // namespace iree

#endif  // IREE_BASE_INTERNAL_FILE_IO_H_
