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
#include <algorithm>  // std::min
#include <mutex>      // std::lock_guard
#include <thread>     // std::this_thread
#include <vector>     // std::vector
#include "parallel/api.h"
#include "parallel/spin_mutex.h"
#include "parallel/thread_scheduler.h"
#include "parallel/thread_team.h"
#include "progress/work.h"
#include "utils/assert.h"
#include "utils/exceptions.h"
#include "utils/function.h"
namespace dt {



//------------------------------------------------------------------------------
// ordered_task
//------------------------------------------------------------------------------
using f1t = function<void(size_t)>;
static f1t noop = [](size_t) {};

class ordered_task : public thread_task {
  private:
    static constexpr size_t READY_TO_START = 0;
    static constexpr size_t STARTING = 1;
    static constexpr size_t READY_TO_ORDER = 2;
    static constexpr size_t ORDERING = 3;
    static constexpr size_t READY_TO_FINISH = 4;
    static constexpr size_t FINISHING = 5;
    static constexpr size_t RECYCLE = 6;
    static constexpr size_t CANCELLED = 42;
    f1t pre_ordered;
    f1t ordered;
    f1t post_ordered;
    size_t state;
    size_t n_iter;

  public:
    ordered_task(f1t pre, f1t ord, f1t post);
    ordered_task(const ordered_task&) = delete;
    ordered_task(ordered_task&&);

    bool ready_to_start()  const noexcept { return state == READY_TO_START; }
    bool ready_to_order()  const noexcept { return state == READY_TO_ORDER; }
    bool ready_to_finish() const noexcept { return state == READY_TO_FINISH; }

    void advance_state();
    void cancel();
    void start_iteration(size_t i);
    void execute(thread_worker*) override;
};


ordered_task::ordered_task(f1t pre, f1t ord, f1t post)
  : pre_ordered(pre? pre : noop),
    ordered(ord? ord : noop),
    post_ordered(post? post : noop),
    state(READY_TO_START),
    n_iter(0) {}

ordered_task::ordered_task(ordered_task&& other)
  : pre_ordered(std::move(other.pre_ordered)),
    ordered(std::move(other.ordered)),
    post_ordered(std::move(other.post_ordered)),
    state(other.state),
    n_iter(other.n_iter) {}

void ordered_task::advance_state() {
  state++;
  if (state == RECYCLE) state = READY_TO_START;
}

void ordered_task::cancel() {
  state = CANCELLED;
}

void ordered_task::start_iteration(size_t i) {
  n_iter = i;
  state++;
}

void ordered_task::execute(thread_worker*) {
  switch (state) {
    case STARTING:  pre_ordered(n_iter); break;
    case ORDERING:  ordered(n_iter); break;
    case FINISHING: post_ordered(n_iter); break;
    default:
      throw RuntimeError() << "Invalid state = " << state; // LCOV_EXCL_LINE
  }
}



//------------------------------------------------------------------------------
// wait_task
//------------------------------------------------------------------------------

// This subclass of `ordered_task` is specifically used for waiting. The state
// inherited from the parent class is completely ignored.
class wait_task : public ordered_task {
  public:
    wait_task();
    void execute(thread_worker*) override;
};

wait_task::wait_task() : ordered_task(nullptr, nullptr, nullptr) {}

void wait_task::execute(thread_worker*) {
  std::this_thread::yield();
}




//------------------------------------------------------------------------------
// ordered_scheduler
//------------------------------------------------------------------------------

class ordered_scheduler : public thread_scheduler {
  friend class ordered;
  private:
    size_t n_tasks;
    size_t n_threads;
    size_t n_iterations;
    std::vector<ordered_task> tasks;
    std::vector<ordered_task*> assigned_tasks;

    // Runtime
    static constexpr size_t NO_THREAD = size_t(-1);
    static constexpr size_t INVALID_THREAD = size_t(-2);
    progress::work& work;
    wait_task waittask;
    dt::spin_mutex mutex;  // 1 byte
    size_t : 56;
    size_t next_to_start;
    size_t next_to_order;
    size_t next_to_finish;
    size_t ordering_thread_index;
    size_t istart;
    size_t iorder;
    size_t ifinish;

  public:
    ordered_scheduler(size_t ntasks, size_t nthreads, size_t niters,
                      progress::work&);
    thread_task* get_next_task(size_t) override;
    void abort_execution() override;
};


ordered_scheduler::ordered_scheduler(size_t ntasks, size_t nthreads,
                                     size_t niters, progress::work& work_)
  : n_tasks(ntasks),
    n_threads(nthreads),
    n_iterations(niters),
    assigned_tasks(nthreads, &waittask),
    work(work_),
    next_to_start(0),
    next_to_order(0),
    next_to_finish(0),
    ordering_thread_index(NO_THREAD),
    istart(0),
    iorder(0),
    ifinish(0) {}


thread_task* ordered_scheduler::get_next_task(size_t ith) {
  if (ith >= n_threads) return nullptr;
  std::lock_guard<spin_mutex> lock(mutex);

  ordered_task* task = assigned_tasks[ith];
  task->advance_state();
  if (ith == ordering_thread_index) {
    ordering_thread_index = NO_THREAD;
    work.set_done_amount(next_to_order);
  }

  // If `iorder`th frame is ready to be ordered, and no other thread is
  // doing ordering right now, then process that frame. Clearing up the
  // "to-be-ordered" queue should always be the highest priority.
  if (ordering_thread_index == NO_THREAD &&
      next_to_order < n_iterations && tasks[iorder].ready_to_order()) {
    ordering_thread_index = ith;
    task = &tasks[iorder];
    task->start_iteration(next_to_order);
    iorder = (++next_to_order) % n_tasks;
  }
  // Otherwise, if there are any tasks that are ready to be finished, then
  // perform those, clearing up the backlog.
  else if (next_to_finish < n_iterations && tasks[ifinish].ready_to_finish()) {
    task = &tasks[ifinish];
    task->start_iteration(next_to_finish);
    ifinish = (++next_to_finish) % n_tasks;
  }
  // Otherwise if there are still tasks in the start queue, and there are
  // tasks available where to execute those, then do the next "start" task.
  else if (next_to_start < n_iterations && tasks[istart].ready_to_start()) {
    task = &tasks[istart];
    task->start_iteration(next_to_start);
    istart = (++next_to_start) % n_tasks;
  }
  // Otherwise we can't do any actual tasks right now. However, if there will
  // be some tasks in the future (not all iterations finished yet), then do a
  // simple wait task until more work becomes available.
  else if (next_to_finish < n_iterations) {
    task = &waittask;
  }
  // Otherwise (next_to_finish == n_iters) there isn't anything left to do:
  // hooray! We allow the worker to go back to sleep by returning nullptr.
  else {
    return nullptr;
  }
  assigned_tasks[ith] = task;

  return task;
}


void ordered_scheduler::abort_execution() {
  std::lock_guard<spin_mutex> lock(mutex);
  next_to_start = n_iterations;
  // next_to_order = n_iterations;
  next_to_finish = n_iterations;
  ordering_thread_index = INVALID_THREAD;
  tasks[iorder].cancel();
}




//------------------------------------------------------------------------------
// ordered
//------------------------------------------------------------------------------

ordered::ordered(ordered_scheduler* s, function<void(ordered*)> f)
  : sch(s), init(f) {}

/**
 * This function finishes initializing the scheduler object, and then runs
 * the scheduler in the thread pool. When this function returns, the entire
 * ordered job will already be completed.
 *
 * In case you're wondering what the crazy hack is this, here's the general
 * idea:
 *
 * The syntax for parallel ordered for-loop looks like this:
 *
 *     dt::parallel_for_ordered(n,
 *       [&](ordered* o) {
 *         ... prepare context ...
 *         o->parallel(
 *           [&](size_t i){ ... pre_ordered ... },
 *           [&](size_t i){ ... ordered ... },
 *           [&](size_t i){ ... post_ordered ... }
 *         );
 *         ... cleanup context ...
 *       });
 *
 * The crucial part is that there is some per-thread (or per-task) context
 * that has to be prepared, then the functions pre_ordered/ordered/post_ordered
 * should run in the current thread team, and only once the job is completely
 * finished should the thread-local context be cleaned up.
 *
 * This feat should be achieved in such a way that there are more task objects
 * than threads. Thus, conceptually what we want to happen is that multiple
 * tasks start executing the outer lambda, then "pause" at the "parallel"
 * section, then the team of threads would jointly execute the parallel section,
 * and only then each task would "unpause" and proceed with cleaning up the
 * thread-local context.
 *
 * In practice we achieve this via deep recursion: the "thread-local" contexts
 * are prepared in master thread one by one. When a new context is created,
 * the triple of functions pre_ordered/ordered/post_ordered (each capturing the
 * context via lambda mechanisms) is pushed onto the vector of ordered tasks.
 * At this point we recursively start creating the next context without
 * returning from the function. This would create a sequence of `n` nested
 * functions; and at the deepest level we call `thpool->execute_job(this)`.
 * Once this function returns, the ordered job is finished and we are free to
 * exit each level of the `init()` function, running the "cleanup context" part
 * of its body.
 */
void ordered::parallel(function<void(size_t)> pre_ordered,
                       function<void(size_t)> do_ordered,
                       function<void(size_t)> post_ordered)
{
  if (sch->n_threads <= 1) {
    MonitorGuard _;
    if (!pre_ordered)  pre_ordered = noop;
    if (!do_ordered)   do_ordered = noop;
    if (!post_ordered) post_ordered = noop;
    for (size_t i = 0; i < sch->n_iterations; ++i) {
      pre_ordered(i);
      do_ordered(i);
      post_ordered(i);
      sch->work.add_done_amount(1);
      if (progress::manager->is_interrupt_occurred()) {
        progress::manager->handle_interrupt();
      }
    }
    return;
  }

  sch->tasks.emplace_back(pre_ordered, do_ordered, post_ordered);
  if (sch->tasks.size() == sch->n_tasks) {
    thpool->execute_job(sch);
  } else {
    init(this);
  }
}


void ordered::set_n_iterations(size_t n) {
  std::lock_guard<dt::spin_mutex> lock(sch->mutex);
  size_t delta = n - sch->n_iterations;
  sch->n_iterations = n;
  sch->work.add_work_amount(delta);
}




//------------------------------------------------------------------------------
// parallel_for_ordered
//------------------------------------------------------------------------------

void parallel_for_ordered(size_t niters, function<void(ordered*)> fn) {
  parallel_for_ordered(niters, NThreads(thpool->size()), fn);
}


void parallel_for_ordered(size_t niters, NThreads NThreads_,
                          function<void(ordered*)> fn)
{
  if (!niters) return;
  dt::progress::work job(niters);
  size_t nthreads = NThreads_.get();

  thpool->instantiate_threads();  // temp fix
  xassert(!thpool->in_parallel_region());
  size_t nthreads0 = thpool->size();
  if (nthreads > nthreads0) nthreads = nthreads0;

  size_t ntasks = std::min(niters, nthreads * 2);
  if (nthreads == 0) ntasks = 1;
  else if (nthreads > ntasks) nthreads = ntasks;
  thread_team tt(nthreads, thpool);
  ordered_scheduler sch(ntasks, nthreads, niters, job);
  ordered octx(&sch, fn);
  fn(&octx);
  job.done();
}



}  // namespace dt
