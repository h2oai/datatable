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
#include <functional>    // std::function
#include <mutex>         // std::mutex
#include "parallel/api_primitives.h"
#include "utils/function.h"
namespace dt {
using std::size_t;



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
 * Return the total number of threads in the thread pool, when called from the
 * master thread, and the total number of threads in a team, otherwise.
 */
size_t num_threads_available();


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



/**
 * Check if the monitor thread is running.
 */
bool is_monitor_enabled() noexcept;


//------------------------------------------------------------------------------
// Parallel constructs
//------------------------------------------------------------------------------

/**
 * Call function `f` exactly once in each thread.
 */
void parallel_region(NThreads, function<void()> f);
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
 * Run parallel loop `for i in range(nrows): f(i)`, with dynamic scheduling.
 */
void parallel_for_dynamic(size_t nrows, std::function<void(size_t)> fn);
void parallel_for_dynamic(size_t nrows, NThreads,
                          std::function<void(size_t)> fn);



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

void parallel_for_ordered(size_t n_iterations, NThreads NThreads_,
                          function<void(ordered*)> fn);

void parallel_for_ordered(size_t n_iterations,
                          function<void(ordered*)> fn);


std::mutex& python_mutex();

}  // namespace dt


//------------------------------------------------------------------------------
// parallel-for-static
//------------------------------------------------------------------------------
#include "parallel/parallel_for_static.h"



#endif
