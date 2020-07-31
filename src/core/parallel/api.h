//------------------------------------------------------------------------------
// Copyright 2019-2020 H2O.ai
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
#include "parallel/python_lock.h"
#include "parallel/thread_job.h"
#include "utils/function.h"
namespace dt {


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


class OrderedJob;
class OrderedTask;

/**
  * Execute loop
  *
  *     for i in range(n_iterations):
  *         start(i)
  *         order(i)
  *         finish(i)
  *
  * where `start` and `finish` are allowed to run in parallel,
  * whereas the `order(i)` parts will run sequentially, in single
  * -threaded mode.
  *
  * Functions `start()`, `order()` and `finish()` must be implemented
  * by a class derived from `OrderedTask`. This function takes a
  * factory method for generating instances of your class. The number
  * of instances generated will depend on the number of iterations
  * and the number of threads.
  */
void parallel_for_ordered(size_t n_iterations, NThreads nthreads,
                          function<std::unique_ptr<OrderedTask>()>);
void parallel_for_ordered(size_t n_iterations,
                          function<std::unique_ptr<OrderedTask>()>);


class OrderedTask : public ThreadTask {
  enum State : size_t;
  private:
    State  state_;
    size_t iter_;
    OrderedJob* parent_job_;  // borrowed ref

  public:
    OrderedTask();
    OrderedTask(const OrderedTask&) = delete;
    OrderedTask(OrderedTask&&) noexcept = default;

    virtual void start(size_t i);
    virtual void order(size_t i);
    virtual void finish(size_t i);
    size_t get_iteration() const noexcept;

    void execute() override;  // ThreadTask's API

    // Change the number of iterations in the ordered job. This may
    // only be called from an ordered section. The new number of
    // iterations cannot be less than the number of iterations that
    // were already ordered.
    //
    // If the new number of iterations is smaller than the original
    // total count, then it is guaranteed that no task will be
    // ordered or finished past `n`, although it is possible that
    // some tasks will have started at an index `n` or above.
    //
    void set_num_iterations(size_t n);
    size_t get_num_iterations() const;
    void wait_until_all_finalized();

  private:
    friend class MultiThreaded_OrderedJob;
    friend class SingleThreaded_OrderedJob;
    void init_parent(OrderedJob* parent);
    bool ready_to_start()  const noexcept;
    bool ready_to_order()  const noexcept;
    bool ready_to_finish() const noexcept;
    bool is_starting()     const noexcept;
    bool is_ordering()     const noexcept;
    bool is_finishing()    const noexcept;
    void advance_state();
    void cancel();
    void start_iteration(size_t i);
};




}  // namespace dt


//------------------------------------------------------------------------------
// parallel-for-static
//------------------------------------------------------------------------------
#include "parallel/parallel_for_static.h"



#endif
