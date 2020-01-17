//------------------------------------------------------------------------------
// Copyright 2019 H2O.ai
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//------------------------------------------------------------------------------
#ifndef dt_PARALLEL_FOR_STATIC_h
#define dt_PARALLEL_FOR_STATIC_h
#include <algorithm>
#include "progress/progress_manager.h"  // dt::progress::progress_manager
#include "parallel/monitor_thread.h"
#include "utils/assert.h"
namespace dt {

// Declared in api.h
size_t this_thread_index();
size_t num_threads_in_pool();
size_t num_threads_in_team();


/**
 * Run a for-loop in parallel, using static scheduling. The iterations
 * will be distributed uniformly across the available threads, each
 * thread will be processing a contiguous sub-range of iterations. The
 * `func` argument should be a function with the signature
 * `void(size_t i)`, and it will be called for all values of `i` in
 * the range [0 .. n_iterations-1]. For example:
 *
 *     dt::parallel_for_static(100, NThreads(4),
 *       [&](size_t i) {
 *         // do something for iteration i
 *       });
 *
 * Each thread will be assigned 25 observations to process:
 *
 *     Thread 0: [ 0:25]
 *     Thread 1: [25:50]
 *     Thread 2: [50:75]
 *     Thread 3: [75:100]
 *
 * The parameter `nthreads` is optional, if omitted it will default to
 * using all available threads.
 *
 * This function must not be called within a parallel region. See
 * `nested_for_static_plain()` instead.
 *
 * Note: as this function doesn't do any chunking, it cannot be interrupted
 * with the SIGINT signal.
 */
template <typename F>
void parallel_for_static_plain(size_t n_iterations, NThreads nthreads, F func) {
  xassert(num_threads_in_team() == 0);
  xassert(n_iterations <= size_t(-1) / nthreads.get());

  // Fast case: the number of iterations is either 0 or 1
  if (n_iterations <= 1) {
    if (n_iterations) {
      func(0);
    }
    return;
  }

  parallel_region(
    nthreads.get(),
    [=] {
      size_t ith = this_thread_index();
      size_t nth = num_threads_in_team();
      size_t i0 = n_iterations * ith / nth;
      size_t i1 = n_iterations * (ith + 1) / nth;
      for (size_t i = i0; i < i1; ++i) {
        func(i);
      }
    });
}



/**
 * Run a for-loop in parallel, using static scheduling with an
 * explicit chunk size. The `func` argument should be a function with
 * the signature `void(size_t i)`, which will be called for all values
 * of `i` in the range [0 .. n_iterations-1]. For example:
 *
 *     dt::parallel_for_static(100, ChunkSize(13), NThreads(4),
 *       [&](size_t i) {
 *         // do something for iteration i
 *       });
 *
 * Here the iterations will be assigned as follows:
 *
 *     Thread 0: [ 0:13] + [52:65]
 *     Thread 1: [13:26] + [65:78]
 *     Thread 2: [26:39] + [78:91]
 *     Thread 3: [39:52] + [91:100]
 *
 * The `nthreads` argument is optional; if omitted all threads in the
 * pool will be used.
 *
 * This function must not be called within another parallel region.
 * See `nested_for_static()` instead.
 */
template <typename F>
void parallel_for_static(size_t n_iterations,
                         ChunkSize chunk_size,
                         NThreads nthreads,
                         F func)
{
  xassert(num_threads_in_team() == 0);
  size_t chunk_size_ = chunk_size.get();
  size_t num_threads = nthreads.get();

  // Fast case: the number of rows is too small compared to the
  // chunk size, no need to start a parallel region
  if (n_iterations <= chunk_size_ || num_threads == 1) {
    // ensures monitor thread is turned off in the end
    MonitorGuard _;
    size_t i0 = 0;
    while (i0 < n_iterations) {
      size_t i1 = std::min(i0 + chunk_size_, n_iterations);
      for (size_t i = i0; i < i1; ++i) {
        func(i);
      }
      i0 += chunk_size_;
      if (progress::manager->is_interrupt_occurred()) {
        i0 = n_iterations;
        progress::manager->handle_interrupt();
      }
    }
    return;
  }

  parallel_region(
    NThreads(num_threads),
    [=] {
      size_t i0 = chunk_size_ * this_thread_index();
      size_t di = chunk_size_ * num_threads;
      while (i0 < n_iterations) {
        size_t i1 = std::min(i0 + chunk_size_, n_iterations);
        for (size_t i = i0; i < i1; ++i) {
          func(i);
        }
        i0 += di;
      if (progress::manager->is_interrupt_occurred()) {
          i0 = n_iterations;
        }
      }
    });
}

template <typename F>
void parallel_for_static(size_t n_iterations, ChunkSize cs, F func) {
  parallel_for_static(n_iterations, cs, NThreads(), func);
}


template <typename F>
void parallel_for_static(size_t n_iterations, F func) {
  parallel_for_static(n_iterations, ChunkSize(),  NThreads(), func);
}


template <typename F>
void parallel_for_static(size_t n_iterations, NThreads nthreads, F func) {
  parallel_for_static(n_iterations, ChunkSize(), nthreads, func);
}


/**
 * Similar to `parallel_for_static_plain(n_iterations, func)`, however this
 * function is expected to be called from within a parallel region.
 *
 * This function will effectively execute the loop
 *
 *     for (size_t i = 0; i < n_iterations; ++i) {
 *       func(i);
 *     }
 *
 * but the actual iterations will be distributed among all threads
 * in the team.
 *
 * There is no implicit barrier at the end of this function, meaning
 * that as each thread finishes its iterations, it will not wait for
 * the other threads to finish theirs. If a barrier is desired, add
 * an explicit call to `dt::barrier()` at the end.
 *
 * Note: as this function doesn't do any chunking, it cannot be interrupted
 * with the SIGINT signal.
 */
template <typename F>
void nested_for_static_plain(size_t n_iterations, F func) {
  size_t ith = this_thread_index();
  size_t nth = num_threads_in_team();
  xassert(nth > 0);

  size_t i0 = n_iterations * ith / nth;
  size_t i1 = n_iterations * (ith + 1) / nth;
  for (size_t i = i0; i < i1; ++i) {
    func(i);
  }
}



/**
 * Similar to `parallel_for_static(n_iterations, chunk_size, func)`,
 * except that this function can only be called within a parallel
 * region.
 */
template <typename F>
void nested_for_static(size_t n_iterations, ChunkSize chunk_size, F func)
{
  size_t chsize = chunk_size.get();
  size_t i0 = chsize * this_thread_index();
  size_t di = chsize * num_threads_in_team();
  while (i0 < n_iterations) {
    size_t i1 = std::min(i0 + chsize, n_iterations);
    for (size_t i = i0; i < i1; ++i) {
      func(i);
    }
    i0 += di;
    if (progress::manager->is_interrupt_occurred()) {
      i0 = n_iterations;
    }
  }
}


template <typename F>
void nested_for_static(size_t n_iterations, F func) {
  nested_for_static(n_iterations, ChunkSize(), func);
}



} // namespace dt
#endif
