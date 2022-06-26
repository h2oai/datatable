//------------------------------------------------------------------------------
// Copyright 2019-2022 H2O.ai
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
//------------------------------------------------------------------------------
#include <algorithm>  // std::min
#include <mutex>      // std::lock_guard
#include <thread>     // std::this_thread
#include <vector>     // std::vector
#include "parallel/api.h"
#include "parallel/spin_mutex.h"
#include "parallel/thread_job.h"
#include "parallel/thread_team.h"
#include "utils/assert.h"
#ifdef NO_DT
  #include <stdexcept>
#else
  #include "progress/work.h"
  #include "utils/exceptions.h"
#endif
#include "utils/function.h"
namespace dt {

using uqOrderedTask = std::unique_ptr<OrderedTask>;


//------------------------------------------------------------------------------
// OrderedJob
//------------------------------------------------------------------------------

class OrderedJob : public ThreadJob {
  public:
    virtual void wait_until_all_finalized() = 0;
    virtual size_t get_num_iterations() const = 0;
    virtual void set_num_iterations(size_t n) = 0;
    virtual void super_ordered(std::function<void()> f) = 0;
};




//------------------------------------------------------------------------------
// OrderedTask
//------------------------------------------------------------------------------

enum OrderedTask::State : size_t {
  READY_TO_START  = 0,
  STARTING        = 1,
  READY_TO_ORDER  = 2,
  ORDERING        = 3,
  READY_TO_FINISH = 4,
  FINISHING       = 5,
  RECYCLE         = 6,
};


OrderedTask::OrderedTask()
  : state_(State::READY_TO_START),
    iter_(0),
    parent_job_(nullptr) {}


void OrderedTask::start(size_t) {}
void OrderedTask::order(size_t) {}
void OrderedTask::finish(size_t) {}

size_t OrderedTask::get_iteration() const noexcept {
  return iter_;
}

size_t OrderedTask::get_num_iterations() const {
  return parent_job_->get_num_iterations();
}

void OrderedTask::set_num_iterations(size_t n) {
  xassert(is_ordering());
  parent_job_->set_num_iterations(n);
}

void OrderedTask::wait_until_all_finalized() {
  parent_job_->wait_until_all_finalized();
}

void OrderedTask::super_ordered(std::function<void()> f) {
  xassert(is_ordering());
  parent_job_->super_ordered(f);
}


void OrderedTask::execute() {
  switch (state_) {
    case State::STARTING:  start(iter_); break;
    case State::ORDERING:  order(iter_); break;
    case State::FINISHING: finish(iter_); break;
    default:
      #ifdef NO_DT
        throw std::runtime_error("Invalid state " + std::to_string(state_)); // LCOV_EXCL_LINE
      #else
        throw RuntimeError() << "Invalid state " << static_cast<int>(state_) // LCOV_EXCL_LINE
      #endif
    ;
  }
}


void OrderedTask::init_parent(OrderedJob* parent) {
  parent_job_ = parent;
}

bool OrderedTask::ready_to_start()  const noexcept { return state_ == State::READY_TO_START; }
bool OrderedTask::ready_to_order()  const noexcept { return state_ == State::READY_TO_ORDER; }
bool OrderedTask::ready_to_finish() const noexcept { return state_ == State::READY_TO_FINISH; }
bool OrderedTask::is_starting()     const noexcept { return state_ == State::STARTING; }
bool OrderedTask::is_ordering()     const noexcept { return state_ == State::ORDERING; }
bool OrderedTask::is_finishing()    const noexcept { return state_ == State::FINISHING; }


void OrderedTask::advance_state() {
  state_ = static_cast<State>(state_ + 1);
  if (state_ == State::RECYCLE) {
    state_ = State::READY_TO_START;
  }
}


void OrderedTask::start_iteration(size_t i) {
  xassert(state_ == State::READY_TO_START);
  iter_ = i;
  state_ = State::STARTING;
}




//------------------------------------------------------------------------------
// NoopTask
//------------------------------------------------------------------------------

// This subclass of `OrderedTask` is specifically used for waiting.
// The state inherited from the parent class is completely ignored.
//
// Even though this class is used for waiting, it does not do any
// waiting explicitly. Instead it finishes almost immediately and
// then the thread tries to re-acquire a new task.
//
// After this task is executed, the parent `OrderedJob` will call
// `.advance_state()` on it. This is ok, as long as we do not expect
// the `state_` of this class to be meaningful.
//
class NoopTask : public OrderedTask {
  public:
    NoopTask() = default;

    void execute() override {
      std::this_thread::yield();
    }
};



//------------------------------------------------------------------------------
// SingleThreaded_OrderedJob
//------------------------------------------------------------------------------

class SingleThreaded_OrderedJob : public OrderedJob {
  private:
    uqOrderedTask task_;
    size_t n_iterations_;
    #ifndef NO_DT
      progress::work* progress_;
    #endif

  public:
    SingleThreaded_OrderedJob(size_t niters,
                              #ifndef NO_DT
                                progress::work* progress,
                              #endif
                              uqOrderedTask&& task)
      : task_(std::move(task)),
        n_iterations_(niters)
        #ifndef NO_DT
          , progress_(progress)
        #endif
    {
      task_->init_parent(this);
    }

    void run() {
      for (size_t i = 0; i < n_iterations_; ++i) {
        task_->iter_ = i;
        task_->state_ = OrderedTask::State::STARTING;   task_->start(i);
        task_->state_ = OrderedTask::State::ORDERING;   task_->order(i);
        task_->state_ = OrderedTask::State::FINISHING;  task_->finish(i);
        #ifndef NO_DT
          progress_->add_done_amount(1);
          progress::manager->check_interrupts_main();
        #endif
      }
    }

    void wait_until_all_finalized() override {}

    size_t get_num_iterations() const override {
      return n_iterations_;
    }

    void set_num_iterations(size_t n) override {
      #ifndef NO_DT
        progress_->add_work_amount(n - n_iterations_);
      #endif
      n_iterations_ = n;
    }

    void super_ordered(std::function<void()> f) override {
      f();
      size_t i = task_->iter_;
      task_->state_ = OrderedTask::State::STARTING;  task_->start(i);
      task_->state_ = OrderedTask::State::ORDERING;  task_->order(i);
    }


    ThreadTask* get_next_task(size_t) override {  // unused
      #ifdef NO_DT
        throw std::runtime_error(__FILE__ + std::string(":") + std::to_string(__LINE__)); // LCOV_EXCL_LINE
      #else
        throw RuntimeError() << __FILE__ << ":" << __LINE__;   // LCOV_EXCL_LINE
      #endif
    }                               // LCOV_EXCL_LINE
};




//------------------------------------------------------------------------------
// MultiThreaded_OrderedJob
//------------------------------------------------------------------------------

class MultiThreaded_OrderedJob : public OrderedJob {
  private:
    size_t n_iterations_;
    size_t n_threads_;
    size_t n_tasks_;
    std::vector<uqOrderedTask> tasks_;
    std::vector<OrderedTask*> assigned_tasks_;

    static constexpr size_t NO_THREAD = size_t(-1);
    static constexpr size_t INVALID_THREAD = size_t(-2);
    #ifndef NO_DT
      progress::work* progress_;
    #endif
    NoopTask noop_task_;
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
    MultiThreaded_OrderedJob(size_t niters,
                             #ifndef NO_DT
                               progress::work* progress,
                             #endif
                             std::vector<uqOrderedTask>&& tasks)
      : n_iterations_(niters),
        n_threads_(num_threads_in_team()),
        n_tasks_(tasks.size()),
        tasks_(std::move(tasks)),
        assigned_tasks_(n_tasks_, &noop_task_),
        #ifndef NO_DT
          progress_(progress),
        #endif
        next_to_start_(0),
        next_to_order_(0),
        next_to_finish_(0),
        ordering_thread_index_(NO_THREAD),
        istart_(0),
        iorder_(0),
        ifinish_(0)
    {
      xassert(n_tasks_ && n_threads_ > 1 && n_iterations_);
      xassert(noop_task_.ready_to_start());
      for (auto& task : tasks_) {
        task->init_parent(this);
      }
    }


    ThreadTask* get_next_task(size_t ith) override {
      if (ith >= n_threads_) return nullptr;
      std::lock_guard<spin_mutex> lock(mutex_);

      OrderedTask* task = assigned_tasks_[ith];
      task->advance_state();  // finish previously assigned task

      if (ith == ordering_thread_index_) {
        ordering_thread_index_ = NO_THREAD;
        #ifndef NO_DT
          progress_->set_done_amount(next_to_order_);
        #endif
      }

      // If `iorder_`th frame is ready to be ordered, and no other thread is
      // doing ordering right now, then process that frame. Clearing up the
      // "to-be-ordered" queue should always be the highest priority.
      if (ordering_thread_index_ == NO_THREAD &&
          next_to_order_ < n_iterations_ && tasks_[iorder_]->ready_to_order()) {
        ordering_thread_index_ = ith;
        task = tasks_[iorder_].get();
        task->advance_state();
        xassert(task->get_iteration() == next_to_order_ && task->is_ordering());
        iorder_ = (++next_to_order_) % n_tasks_;
      }
      // Otherwise, if there are any tasks that are ready to be finished, then
      // perform those, clearing up the backlog.
      else if (next_to_finish_ < n_iterations_ && tasks_[ifinish_]->ready_to_finish()) {
        task = tasks_[ifinish_].get();
        task->advance_state();
        xassert(task->get_iteration() == next_to_finish_ && task->is_finishing());
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
        task = &noop_task_;
      }
      // Otherwise (next_to_finish_ == n_iters) there isn't anything left to do:
      // hooray! We allow the worker to go back to sleep by returning nullptr.
      else {
        return nullptr;
      }
      assigned_tasks_[ith] = task;

      return task;
    }


    void abort_execution() override {
      std::lock_guard<spin_mutex> lock(mutex_);
      next_to_start_ = n_iterations_;
      next_to_order_ = n_iterations_;
      next_to_finish_ = n_iterations_;
      ordering_thread_index_ = INVALID_THREAD;
    }


    size_t get_num_iterations() const override {
      return n_iterations_;
    }

    void set_num_iterations(size_t n) override {
      std::lock_guard<dt::spin_mutex> lock(mutex_);
      xassert(n >= next_to_order_);
      #ifndef NO_DT
        progress_->add_work_amount(n - n_iterations_);
      #endif
      n_iterations_ = n;
    }


    void wait_until_all_finalized() override {
      while (any_task_finishing()) {
        std::this_thread::yield();
      }
    }


    void super_ordered(std::function<void()> f) override {
      {
        std::lock_guard<dt::spin_mutex> lock(mutex_);
        // prevent new tasks from starting
        next_to_start_ = n_iterations_;
      }
      // Wait for all other tasks to complete their steps
      while (any_task_starting_or_finishing()) {
        std::this_thread::yield();
      }
      // At this moment all other threads should be executing
      // a NoopTask. Just to be sure, we will lock the mutex,
      // preventing them from acquiring any new tasks.
      {
        std::lock_guard<dt::spin_mutex> lock(mutex_);
        // execute the payload function
        f();

        // And lastly, resume the iterations
        xassert(next_to_finish_ == next_to_order_ - 1);
        next_to_start_ = next_to_order_ = next_to_finish_;
        istart_ = iorder_ = ifinish_;
        ordering_thread_index_ = NO_THREAD;
        tasks_[istart_]->state_ = OrderedTask::State::READY_TO_START;
        assigned_tasks_[dt::this_thread_index()] = &noop_task_;
        for (const auto& task : tasks_) {
          if (task->ready_to_order()) {
            xassert(task->iter_ >= next_to_start_);
            task->state_ = OrderedTask::State::READY_TO_START;
          }
        }
        for (size_t i = 0; i < n_tasks_; ++i) {
          xassert(assigned_tasks_[i] == &noop_task_);
        }
      }
    }

  private:
    // Tasks that are READY_TO_FINISH are considered "finishing" too,
    // because they may enter the FINISHING stage at any moment.
    bool any_task_finishing() const {
      std::lock_guard<dt::spin_mutex> lock(mutex_);
      for (const auto& task : tasks_) {
        if (task->is_finishing())    return true;
        if (task->ready_to_finish()) return true;
      }
      return false;
    }

    bool any_task_starting_or_finishing() const {
      std::lock_guard<dt::spin_mutex> lock(mutex_);
      for (const auto& task : tasks_) {
        if (task->is_finishing())    return true;
        if (task->is_starting())     return true;
        if (task->ready_to_finish()) return true;
      }
      return false;
    }
};




//------------------------------------------------------------------------------
// parallel_for_ordered
//------------------------------------------------------------------------------

void parallel_for_ordered(size_t niters, NThreads nthreads0,
                          function<uqOrderedTask()> task_factory)
{
  if (!niters) return;
  #ifndef NO_DT
    progress::work progress(niters);
  #endif
  size_t nthreads = nthreads0.get();

  size_t ntasks = std::min(niters, nthreads * 3/2);
  if (nthreads > ntasks) nthreads = ntasks;

  if (nthreads == 1) {
    SingleThreaded_OrderedJob job(niters,
                                  #ifndef NO_DT
                                    &progress,
                                  #endif
                                  task_factory());
    job.run();
  }
  else {
    ThreadTeam thteam(nthreads, thpool);
    std::vector<uqOrderedTask> tasks;
    tasks.reserve(ntasks);
    for (size_t i = 0; i < ntasks; ++i) {
      tasks.push_back(task_factory());
    }
    MultiThreaded_OrderedJob job(niters,
                                 #ifndef NO_DT
                                   &progress,
                                 #endif
                                 std::move(tasks));
    thpool->execute_job(&job);
  }
  #ifndef NO_DT
    progress.done();
  #endif
}


void parallel_for_ordered(size_t niters,
                          function<uqOrderedTask()> task_factory)
{
  parallel_for_ordered(niters, NThreads(), task_factory);
}



}  // namespace dt
