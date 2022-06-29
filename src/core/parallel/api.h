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
#include <memory>
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
 * main thread, and the total number of threads in a team, otherwise.
 */
size_t num_threads_available();


/**
 * Return the index of the current thread, similar to `omp_get_thread_num()`.
 * The value returned is between 0 and `num_threads_in_team() - 1`, or
 * `size_t(-1)` for the main thread.
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


/**
  * This class facilitates execution of ordered for-loops. A user
  * is expected to derive from this class, overriding methods
  * `start(i)`, `order(i)` and `finish(i)`; and then call
  * `dt::parallel_for_ordered()` supplying a factory function to
  * create instances of the derived class.
  *
  * The class also has several methods for controlling the execution
  * of the orderer loop. See their descriptions below.
  */
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

    // (This method may only be called from an ordered section).
    // Change the number of iterations in the ordered job. The new
    // number of iterations cannot be less than the number of
    // iterations that were already ordered.
    //
    // If the new number of iterations is smaller than the original
    // total count, then it is guaranteed that no task will be
    // ordered or finished past `n`, although it is possible that
    // some tasks will have started at an index `n` or above.
    //
    void set_num_iterations(size_t n);
    size_t get_num_iterations() const;

    // (This method may only be called from an ordered section).
    // By the time this method returns, all tasks with the index
    // less than the current will have completed their "finish"
    // stage. Furthermore, no new tasks will start a "finish" stage
    // until the end of the enclosing "ordered" step.
    //
    void wait_until_all_finalized();

    // (This method may only be called from an ordered section).
    // This method performs the following sequence of actions:
    //   - blocks new tasks from entering the "start" stage;
    //   - waits until there are no tasks executing either the
    //     "start" or "finish" stages;
    //   - executes the payload function `f()`;
    //   - resumes the multithreaded execution, making sure that all
    //     iterations beginning with the current will be re"start"ed.
    //     More specifically, the current iteration that was
    //     executing an ordered section, will not finish. Instead,
    //     this iteration will start again from the "start" step, then
    //     execute "ordered" again, and only after that it will
    //     "finish".
    //
    // Thus, this method creates a region of execution which behaves
    // as-if the ordered loop was cancelled, then f() executed in a
    // single-threaded environment, then the ordered loop restarted
    // from the same iteration where it broke off.
    //
    // The programmer must take care not to create an infinite loop
    // by repeatedly calling `super_ordered` on each execution of
    // the same task.
    //
    void super_ordered(std::function<void()> f);

    size_t get_iteration() const noexcept;
    bool is_starting()  const noexcept;
    bool is_ordering()  const noexcept;
    bool is_finishing() const noexcept;

    void execute() override;  // ThreadTask's API

  private:
    friend class MultiThreaded_OrderedJob;
    friend class SingleThreaded_OrderedJob;
    void init_parent(OrderedJob* parent);
    bool ready_to_start()  const noexcept;
    bool ready_to_order()  const noexcept;
    bool ready_to_finish() const noexcept;
    void advance_state();
    void start_iteration(size_t i);
};




}  // namespace dt


//------------------------------------------------------------------------------
// parallel-for-static
//------------------------------------------------------------------------------
#include "parallel/parallel_for_static.h"



#endif
