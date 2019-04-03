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
#ifndef dt_PARALLEL_API_h
#define dt_PARALLEL_API_h
#include "parallel/thread_pool.h"
#include "utils/function.h"
namespace dt {



/**
 * Return the current number of threads in the pool.
 */
size_t get_num_threads();


/**
 * Return the index of the current thread: from 0 to `get_num_threads() - 1`.
 * For the master thread this will return `size_t(-1)`.
 */
size_t get_thread_num();


/**
 * Return the number of concurrent threads supported by the machine. This
 * value is approximate. If the number of concurrent threads is unknown,
 * this function returns 1.
 */
size_t get_hardware_concurrency() noexcept;


/**
 * Call function `f` exactly once in each thread.
 */
void parallel_region(size_t nthreads, function<void()> f);
void parallel_region(function<void()> f);


/**
 * Run parallel loop `for i in range(nrows): f(i)`.
 */
void _parallel_for_static(size_t nrows, size_t min_chunk_size,
                          function<void(size_t, size_t)> fn);


template <typename F>
void parallel_for_static(size_t nrows, F f) {
  _parallel_for_static(nrows, 4096,
    [&](size_t i0, size_t i1) {
      for (size_t i = i0; i < i1; ++i) f(i);
    });
}

template <typename F>
void parallel_for_static(size_t nrows, size_t min_chunk_size, F f) {
  _parallel_for_static(nrows, min_chunk_size,
    [&](size_t i0, size_t i1) {
      for (size_t i = i0; i < i1; ++i) f(i);
    });
}


}  // namespace dt
#endif
