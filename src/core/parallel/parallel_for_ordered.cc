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
#include <algorithm>  // std::min
#include <mutex>      // std::lock_guard
#include <thread>     // std::this_thread
#include <vector>     // std::vector
#include "parallel/api.h"
#include "parallel/spin_mutex.h"
#include "parallel/thread_job.h"
#include "parallel/thread_team.h"
#include "progress/work.h"
#include "utils/assert.h"
#include "utils/exceptions.h"
#include "utils/function.h"
namespace dt {



//------------------------------------------------------------------------------
// OrderedTask
//------------------------------------------------------------------------------
using f1t = function<void(size_t)>;
static f1t noop = [](size_t) {};

class OrderedTask : public ThreadTask {
  #if DT_DEBUG
    friend class OrderedJob;
  #endif
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
    OrderedTask(f1t pre, f1t ord, f1t post);
    OrderedTask(const OrderedTask&) = delete;
    OrderedTask(OrderedTask&&);

    bool ready_to_start()  const noexcept { return state == READY_TO_START; }
    bool ready_to_order()  const noexcept { return state == READY_TO_ORDER; }
    bool ready_to_finish() const noexcept { return state == READY_TO_FINISH; }
    bool is_finishing()    const noexcept { return state == FINISHING; }
    size_t iteration()     const noexcept { return n_iter; }

    void advance_state();
    void cancel();
    void start_iteration(size_t i);
    void execute() override;
};


OrderedTask::OrderedTask(f1t pre, f1t ord, f1t post)
  : pre_ordered(pre? pre : noop),
    ordered(ord? ord : noop),
    post_ordered(post? post : noop),
    state(READY_TO_START),
    n_iter(0) {}

OrderedTask::OrderedTask(OrderedTask&& other)
  : pre_ordered(std::move(other.pre_ordered)),
    ordered(std::move(other.ordered)),
    post_ordered(std::move(other.post_ordered)),
    state(other.state),
    n_iter(other.n_iter) {}

void OrderedTask::advance_state() {
  state++;
  if (state == RECYCLE) state = READY_TO_START;
}

void OrderedTask::cancel() {
  state = CANCELLED;
}

void OrderedTask::start_iteration(size_t i) {
  xassert(state == READY_TO_START);
  n_iter = i;
  state = STARTING;
}

void OrderedTask::execute() {
  switch (state) {
    case STARTING:  pre_ordered(n_iter); break;
    case ORDERING:  ordered(n_iter); break;
    case FINISHING: post_ordered(n_iter); break;
    default:
      throw RuntimeError() << "Invalid state = " << state; // LCOV_EXCL_LINE
  }
}



//------------------------------------------------------------------------------
// WaitTask
//------------------------------------------------------------------------------

// This subclass of `OrderedTask` is specifically used for waiting. The state
// inherited from the parent class is completely ignored.
class WaitTask : public OrderedTask {
  public:
    WaitTask();
    void execute() override;
};

WaitTask::WaitTask() : OrderedTask(nullptr, nullptr, nullptr) {}

void WaitTask::execute() {
  std::this_thread::yield();
}




//------------------------------------------------------------------------------
// OrderedJob
//------------------------------------------------------------------------------

class OrderedJob : public ThreadJob {
  friend class ordered;
  private:
    size_t n_tasks_;
    size_t n_threads_;
    size_t n_iterations_;
    std::vector<std::unique_ptr<OrderedTask>> tasks_;
    std::vector<OrderedTask*> assigned_tasks_;

    // Runtime
    static constexpr size_t NO_THREAD = size_t(-1);
    static constexpr size_t INVALID_THREAD = size_t(-2);
    progress::work& work_;
    WaitTask wait_task_;
    mutable dt::spin_mutex mutex_;  // 1 byte
    size_t : 56;
    size_t next_to_start_;
    size_t next_to_order_;
    size_t next_to_finish_;
    size_t ordering_thread_index_;
    size_t istart_;
    size_t iorder_;
    size_t ifinish_;

  public:
    OrderedJob(size_t ntasks, size_t nthreads, size_t niters, progress::work&);
    ThreadTask* get_next_task(size_t) override;
    void abort_execution() override;
    void wait_until_all_finalized() const;
};


OrderedJob::OrderedJob(size_t ntasks, size_t nthreads,
                       size_t niters, progress::work& work)
  : n_tasks_(ntasks),
    n_threads_(nthreads),
    n_iterations_(niters),
    assigned_tasks_(nthreads, &wait_task_),
    work_(work),
    next_to_start_(0),
    next_to_order_(0),
    next_to_finish_(0),
    ordering_thread_index_(NO_THREAD),
    istart_(0),
    iorder_(0),
    ifinish_(0) {}


ThreadTask* OrderedJob::get_next_task(size_t ith) {
  if (ith >= n_threads_) return nullptr;
  std::lock_guard<spin_mutex> lock(mutex_);

  OrderedTask* task = assigned_tasks_[ith];
  task->advance_state();  // finish previously assigned task

  if (ith == ordering_thread_index_) {
    ordering_thread_index_ = NO_THREAD;
    work_.set_done_amount(next_to_order_);
  }

  // If `iorder_`th frame is ready to be ordered, and no other thread is
  // doing ordering right now, then process that frame. Clearing up the
  // "to-be-ordered" queue should always be the highest priority.
  if (ordering_thread_index_ == NO_THREAD &&
      next_to_order_ < n_iterations_ && tasks_[iorder_]->ready_to_order()) {
    ordering_thread_index_ = ith;
    task = tasks_[iorder_].get();
    task->advance_state();
    xassert(task->n_iter == next_to_order_ &&
            task->state == OrderedTask::ORDERING);
    iorder_ = (++next_to_order_) % n_tasks_;
  }
  // Otherwise, if there are any tasks that are ready to be finished, then
  // perform those, clearing up the backlog.
  else if (next_to_finish_ < n_iterations_ && tasks_[ifinish_]->ready_to_finish()) {
    task = tasks_[ifinish_].get();
    task->advance_state();
    xassert(task->n_iter == next_to_finish_ &&
            task->state == OrderedTask::FINISHING);
    ifinish_ = (++next_to_finish_) % n_tasks_;
  }
  // Otherwise if there are still tasks in the start queue, and there are
  // tasks available where to execute those, then do the next "start" task.
  else if (next_to_start_ < n_iterations_ && tasks_[istart_]->ready_to_start()) {
    task = tasks_[istart_].get();
    task->start_iteration(next_to_start_);
    istart_ = (++next_to_start_) % n_tasks_;
  }
  // Otherwise we can't do any actual tasks right now. However, if there will
  // be some tasks in the future (not all iterations finished yet), then do a
  // simple wait task until more work becomes available.
  else if (next_to_finish_ < n_iterations_) {
    task = &wait_task_;
  }
  // Otherwise (next_to_finish_ == n_iters) there isn't anything left to do:
  // hooray! We allow the worker to go back to sleep by returning nullptr.
  else {
    return nullptr;
  }
  assigned_tasks_[ith] = task;

  return task;
}


void OrderedJob::abort_execution() {
  std::lock_guard<spin_mutex> lock(mutex_);
  next_to_start_ = n_iterations_;
  // next_to_order_ = n_iterations_;
  next_to_finish_ = n_iterations_;
  ordering_thread_index_ = INVALID_THREAD;
  tasks_[iorder_]->cancel();
}


/**
  * This should be called from within the "ordered" section only.
  * This function will block until all tasks that are currently in
  * READY_TO_FINISH or FINISHING state are completed.
  *
  * This function is called by the thread which is performing an
  * ORDERING task, which means no new tasks can become
  * READY_TO_FINISH in the meanwhile.
  */
void OrderedJob::wait_until_all_finalized() const {
  if (n_threads_ == 1) return;
  size_t ordering_iter;
  {
    std::lock_guard<dt::spin_mutex> lock(mutex_);
    xassert(dt::this_thread_index() == ordering_thread_index_);
    // next_to_order_ was incremented when the ordering task was
    // started, so the iteration number that is currently being
    // ordered is 1 less than `next_to_order_`.
    ordering_iter = next_to_order_ - 1;
  }
  // Helper function: returns true if there are no tasks in the
  // FINISHING stage, and false otherwise.
  auto no_tasks_finishing = [&]() -> bool {
    for (const auto& task : tasks_) {
      if (task->is_finishing()) return false;
    }
    return true;
  };
  // Busy-wait loop until all eligible tasks have finished their
  // "post-ordered" section.
  while (true) {
    std::this_thread::yield();
    std::lock_guard<dt::spin_mutex> lock(mutex_);
    // when `next_to_finish_` becomes equal to the current ordering
    // iteration, it means all iterations that were READY_TO_FINISH
    // has at least entered the FINISHING stage.
    if (next_to_finish_ == ordering_iter && no_tasks_finishing()) {
      break;
    }
  }
}




//------------------------------------------------------------------------------
// ordered
//------------------------------------------------------------------------------

ordered::ordered(OrderedJob* s, function<void(ordered*)> f)
  : job_(s), init(f) {}

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
  if (job_->n_threads_ <= 1) {
    if (!pre_ordered)  pre_ordered = noop;
    if (!do_ordered)   do_ordered = noop;
    if (!post_ordered) post_ordered = noop;
    for (size_t i = 0; i < job_->n_iterations_; ++i) {
      pre_ordered(i);
      do_ordered(i);
      post_ordered(i);
      job_->work_.add_done_amount(1);
      progress::manager->check_interrupts_main();
    }
    return;
  }

  job_->tasks_.emplace_back(
      new OrderedTask(pre_ordered, do_ordered, post_ordered)
  );
  if (job_->tasks_.size() == job_->n_tasks_) {
    thpool->execute_job(job_);
  } else {
    init(this);
  }
}


void ordered::set_n_iterations(size_t n) {
  std::lock_guard<dt::spin_mutex> lock(job_->mutex_);
  size_t delta = n - job_->n_iterations_;
  job_->n_iterations_ = n;
  job_->work_.add_work_amount(delta);
}


void ordered::wait_until_all_finalized() const {
  job_->wait_until_all_finalized();
}


size_t ordered::get_n_iterations() const {
  std::lock_guard<dt::spin_mutex> lock(job_->mutex_);
  return job_->n_iterations_;
}

size_t ordered::current_iteration() const {
  return job_->assigned_tasks_[dt::this_thread_index()]->iteration();
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
  xassert(!thpool->in_parallel_region());
  dt::progress::work job(niters);
  size_t nthreads = NThreads_.get();

  size_t ntasks = std::min(niters, nthreads * 2);
  if (nthreads > ntasks) nthreads = ntasks;
  ThreadTeam tt(nthreads, thpool);
  OrderedJob sch(ntasks, nthreads, niters, job);
  ordered octx(&sch, fn);
  fn(&octx);
  job.done();
}


void parallel_for_ordered2(size_t niters, NThreads nthreads,
                          function<std::unique_ptr<OrderedTask>()> factory)
{
  if (!niters) return;
  dt::progress::work job(niters);
  size_t nth = nthreads.get();

  size_t ntasks = std::min(niters, nth * 3/2);
  if (nth > ntasks) nth = ntasks;

  ThreadTeam tt(nth, thpool);
}




}  // namespace dt
