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

#ifndef IREE_TASK_TUNING_H_
#define IREE_TASK_TUNING_H_

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

// Maximum number of workers that an executor can manage.
// A 64 worker hard limit is based on us using uint64_t as a bitmask to select
// workers. It's easy to go smaller (just use fewer bits) if it's known that
// only <64 will ever be used (such as for devices with 2 cores).
#define IREE_TASK_EXECUTOR_MAX_WORKER_COUNT (64)

// Initial number of slice tasks that are allocated in the executor pool.
// Increasing this number will decrease initial allocation storms in cases of
// extremely wide fan-out (many dispatches with many thousands of slices) at the
// cost of a higher minimum memory consumption.
//
// Set to zero if sliced dispatches will not be used or will be allocated by the
// caller to avoid fixed overhead associated with the internal executor pool.
#define IREE_TASK_EXECUTOR_INITIAL_SLICE_RESERVATION_PER_WORKER (0)

// Initial number of shard tasks that are allocated in the executor pool.
// Increasing this number will decrease initial allocation storms in cases of
// extremely wide concurrency regions (many dispatches running at the same time)
// at the cost of a higher minimum memory consumption.
#define IREE_TASK_EXECUTOR_INITIAL_SHARD_RESERVATION_PER_WORKER (4)

// Maximum number of simultaneous waits an executor may perform as part of a
// wait-any operation. A larger value may enable better wake coalescing by the
// kernel. This is only a count limiting wait tasks that have been scheduled and
// been promoted to the root executor waiting list. There may be any number of
// waits deeper in the pipeline so long as they don't all become ready
// simultaneously.
//
// Realistically, though, if we have more than 64 outstanding **root** waits
// it's hard to reason about if/when the executor queue could make forward
// progress and indicates a possible error in task assignment.
//
// Also, the underlying iree_wait_set_t may not support more than 64 handles on
// certain platforms without emulation. Trying to keep us on the fast-path
// with a reasonable number seems fine for now until we have a need for more.
//
// NOTE: we reserve 1 wait handle for our own internal use. This allows us to
// wake the coordination worker when new work is submitted from external
// sources.
#define IREE_TASK_EXECUTOR_MAX_OUTSTANDING_WAITS (64 - 1)

// Allows for dividing the total number of attempts that a worker will make to
// steal tasks from other workers. By default all other workers will be
// attempted while setting this to 2, for example, will try for only half of
// the available workers.
#define IREE_TASK_EXECUTOR_MAX_THEFT_ATTEMPTS_DIVISOR (1)

// Maximum number of tasks that will be stolen in one go from another worker.
//
// Too few tasks will cause additional overhead as the worker repeatedly sips
// away tasks and when it does get tasks it may suffer spatial locality cache
// issues as it is effectively walking backwards in memory to both touch the
// tasks and - a much larger impact - running tasks that themselves are walking
// orders of magnitude more memory backwards.
//
// Too many tasks will cause additional latency on workers that may interfere
// with higher level scheduling; for example, if a worker runs out of tasks and
// immediately steals 8000 of them from another worker it's going to take until
// those 8000 complete before any work that arrives specifically for the worker
// is able to start processing.
//
// In real-time systems too few tasks is better (slightly more work for much
// lower variance in execution) while in batch mode systems too many tasks is
// better (as latencies don't matter so long as throughput is maximized).
#define IREE_TASK_EXECUTOR_MAX_THEFT_TASK_COUNT \
  IREE_TASK_EXECUTOR_MAX_WORKER_COUNT

// Number of tiles that will be batched into a single slice along each XYZ dim.
//
// Larger numbers reduce overhead and ensure that more tiles are executed
// locally on the same worker (== shared caches) while also increasing potential
// latency as work-stealing (always per-slice) is less effective.
//
// Numbers >1 on Y and Z can be used to cluster tiles together within the same
// slice when spatial coherence outside of the X dimension is useful.
//
// The current usage of this is provisional; we may do all of this from the
// compiler and want this behavior to be relatively fixed so that we can predict
// it better. The only thing we want to be introducing here is flexibility for
// when worker topology differs at runtime from what is knowable during offline
// compilation.
#define IREE_TASK_DISPATCH_TILES_PER_SLICE_X (8)
#define IREE_TASK_DISPATCH_TILES_PER_SLICE_Y (1)
#define IREE_TASK_DISPATCH_TILES_PER_SLICE_Z (1)

// Number of tiles that will be batched into a single reservation from the grid.
// This is a maximum; if there are fewer tiles that would otherwise allow for
// maximum parallelism then this may be ignored.
//
// The more tiles reserved at a time the higher the chance for latency to
// increase as many reserved tiles are held up on one worker while another may
// have otherwise been able to steal them and help finish them sooner.
//
// The fewer tiles reserved at a time the higher the chance for cache-locality
// destroying behavior where multiple workers all stomp on the same cache lines
// (as say worker 0 and worker 1 both fight over sequential tiles adjacent in
// memory).
#define IREE_TASK_DISPATCH_MAX_TILES_PER_SHARD_RESERVATION (8)

// Whether to enable per-tile colors for each tile tracing zone based on the
// tile grid xyz. Not cheap and can be disabled to reduce tracing overhead.
// TODO(#4017): make per-tile color tracing fast enough to always have on.
#define IREE_TASK_TRACING_PER_TILE_COLORS 1

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // IREE_TASK_TUNING_H_
