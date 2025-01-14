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

#include "iree/task/testing/task_test.h"
#include "iree/testing/gtest.h"
#include "iree/testing/status_matchers.h"

namespace {

class TaskNopTest : public TaskTest {};

TEST_F(TaskNopTest, Issue) {
  iree_task_nop_t task;
  iree_task_nop_initialize(&scope_, &task);
  IREE_ASSERT_OK(SubmitTasksAndWaitIdle(&task.header, &task.header));
}

}  // namespace
