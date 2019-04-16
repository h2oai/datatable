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
#include <cstddef>
#include "utils/function.h"
namespace dt {
using std::size_t;

// Private
void _parallel_for_static(size_t, size_t, size_t,
                          function<void(size_t, size_t)>);


//------------------------------------------------------------------------------
// Generic thread information
//------------------------------------------------------------------------------

/**
 * Return the total number of threads in the thread pool. This is roughly
 * equivalent to `omp_get_max_threads()`.
 *
 * This function returns the same value regardless of whether the thread pool
 * has actually spawned all the threads or not.
 */
size_t num_threads_in_pool();


/**
 * Return the number of threads in the currently executed parallel region. This
 * is similar to `omp_get_num_threads()`.
 *
 * The return value is 0 if no parallel region is currently being executed,
 * otherwise the value is between 1 and `num_threads_in_pool()`. The number of
 * threads in a team can be less than the number of threads in the pool if
 * either the user explicitly requested to use less threads, or if the number
 * of iterations was small enough that using all of them was deamed suboptimal.
 */
size_t num_threads_in_team();


/**
 * Return the index of the current thread, similar to `omp_get_thread_num()`.
 * The value returned is between 0 and `num_threads_in_team() - 1`, or
 * `size_t(-1)` for the master thread.
 */
size_t this_thread_index();


/**
 * Return the number of concurrent threads supported by the machine. This
 * value is approximate. If the number of concurrent threads is unknown,
 * this function returns 1.
 */
size_t get_hardware_concurrency() noexcept;



//------------------------------------------------------------------------------
// Parallel constructs
//------------------------------------------------------------------------------

/**
 * Call function `f` exactly once in each thread.
 */
void parallel_region(size_t nthreads, function<void()> f);
void parallel_region(function<void()> f);


/**
 * Make all threads within the current team wait until all threads have
 * arrived to this point.
 * Note: it is the user's responsibility to ensure that all threads CAN arrive
 * at the barrier. If not, a deadlock will occur as the threads will be waiting
 * for all the team to arrive before they could proceed.
 */
void barrier();


/**
 * Run parallel loop `for i in range(nrows): f(i)`, with static scheduling.
 */
template <typename F>
void parallel_for_static(size_t nrows, F f) {
  _parallel_for_static(nrows, 4096, dt::num_threads_in_pool(),
    [&](size_t i0, size_t i1) {
      for (size_t i = i0; i < i1; ++i) f(i);
    });
}

template <typename F>
void parallel_for_static(size_t nrows, size_t chunk_size, F f) {
  _parallel_for_static(nrows, chunk_size, dt::num_threads_in_pool(),
    [&](size_t i0, size_t i1) {
      for (size_t i = i0; i < i1; ++i) f(i);
    });
}


template <typename F>
void parallel_for_static(size_t nrows, size_t chunk_size, size_t nthreads,
                         F f) {
  _parallel_for_static(nrows, chunk_size, nthreads,
    [&](size_t i0, size_t i1) {
      for (size_t i = i0; i < i1; ++i) f(i);
    });
}


/**
 * Run parallel loop `for i in range(nrows): f(i)`, with dynamic scheduling.
 */
void parallel_for_dynamic(size_t nrows, function<void(size_t)> fn);



/**
 * Execute loop
 *
 *     for i in range(n_iterations):
 *         pre-ordered(i)
 *         ordered(i)
 *         post-ordered(i)
 *
 * where `pre-ordered` and `post-ordered` are allowed to run in parallel,
 * whereas the `ordered(i)` parts will run sequentially, in single-threaded
 * mode.
 */
class ordered_scheduler;
class ordered {
  ordered_scheduler* sch;
  function<void(ordered*)> init;
  public:
    ordered(ordered_scheduler*, function<void(ordered*)>);
    void parallel(function<void(size_t)> pre_ordered,
                  function<void(size_t)> do_ordered,
                  function<void(size_t)> post_ordered);
    void set_n_iterations(size_t n);
};

void parallel_for_ordered(size_t n_iterations, size_t n_threads,
                          function<void(ordered*)> fn);


}  // namespace dt
#endif
