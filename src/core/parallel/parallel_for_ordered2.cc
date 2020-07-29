//------------------------------------------------------------------------------
// Copyright 2019-2020 H2O.ai
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
#include "progress/work.h"
#include "utils/assert.h"
#include "utils/exceptions.h"
#include "utils/function.h"
namespace dt {

using uqOrderedTask = std::unique_ptr<OrderedTask2>;


//------------------------------------------------------------------------------
// OrderedJob2
//------------------------------------------------------------------------------

class OrderedJob2 : public ThreadJob {
  public:
    // This should be called from within an "ordered" section only.
    // This function will block until all tasks that are currently in
    // READY_TO_FINISH or FINISHING state are completed.
    virtual void wait_until_all_finalized() = 0;

    virtual size_t get_num_iterations() const = 0;
    virtual void set_num_iterations(size_t n) = 0;
};




//------------------------------------------------------------------------------
// OrderedTask2
//------------------------------------------------------------------------------

enum OrderedTask2::State : size_t {
  READY_TO_START  = 0,
  STARTING        = 1,
  READY_TO_ORDER  = 2,
  ORDERING        = 3,
  READY_TO_FINISH = 4,
  FINISHING       = 5,
  RECYCLE         = 6,
};


OrderedTask2::OrderedTask2()
  : state_(State::READY_TO_START),
    iter_(0),
    parent_job_(nullptr) {}


void OrderedTask2::start(size_t) {}
void OrderedTask2::order(size_t) {}
void OrderedTask2::finish(size_t) {}

size_t OrderedTask2::get_iteration() const noexcept {
  return iter_;
}

size_t OrderedTask2::get_num_iterations() const {
  return parent_job_->get_num_iterations();
}

void OrderedTask2::set_num_iterations(size_t n) {
  parent_job_->set_num_iterations(n);
}

void OrderedTask2::wait_until_all_finalized() {
  parent_job_->wait_until_all_finalized();
}


void OrderedTask2::execute() {
  switch (state_) {
    case State::STARTING:  start(iter_); break;
    case State::ORDERING:  order(iter_); break;
    case State::FINISHING: finish(iter_); break;
    default:
      throw RuntimeError() << "Invalid state " << static_cast<int>(state_); // LCOV_EXCL_LINE
  }
}


void OrderedTask2::init_parent(OrderedJob2* parent) {
  parent_job_ = parent;
}

bool OrderedTask2::ready_to_start()  const noexcept { return state_ == State::READY_TO_START; }
bool OrderedTask2::ready_to_order()  const noexcept { return state_ == State::READY_TO_ORDER; }
bool OrderedTask2::ready_to_finish() const noexcept { return state_ == State::READY_TO_FINISH; }
bool OrderedTask2::is_starting()     const noexcept { return state_ == State::STARTING; }
bool OrderedTask2::is_ordering()     const noexcept { return state_ == State::ORDERING; }
bool OrderedTask2::is_finishing()    const noexcept { return state_ == State::FINISHING; }


void OrderedTask2::advance_state() {
  state_ = static_cast<State>(state_ + 1);
  if (state_ == State::RECYCLE) {
    state_ = State::READY_TO_START;
  }
}


void OrderedTask2::start_iteration(size_t i) {
  xassert(state_ == State::READY_TO_START);
  iter_ = i;
  state_ = State::STARTING;
}




//------------------------------------------------------------------------------
// WaitTask2
//------------------------------------------------------------------------------

// This subclass of `OrderedTask2` is specifically used for waiting. The state
// inherited from the parent class is completely ignored.
class WaitTask2 : public OrderedTask2 {
  public:
    WaitTask2() = default;

    void execute() override {
      std::this_thread::yield();
    }
};



//------------------------------------------------------------------------------
// SingleThreaded_OrderedJob
//------------------------------------------------------------------------------

class SingleThreaded_OrderedJob : public OrderedJob2 {
  private:
    uqOrderedTask task_;
    size_t n_iterations_;
    progress::work* progress_;

  public:
    SingleThreaded_OrderedJob(size_t niters, progress::work* progress,
                              uqOrderedTask&& task)
      : task_(std::move(task)),
        n_iterations_(niters),
        progress_(progress)
    {
      task_->init_parent(this);
    }

    void run() {
      for (size_t i = 0; i < n_iterations_; ++i) {
        // Note: we do not bother to advance the task's state_ since
        //       it won't be used by anyone
        task_->iter_ = i;
        task_->start(i);
        task_->order(i);
        task_->finish(i);
        progress_->add_done_amount(1);
        progress::manager->check_interrupts_main();
      }
    }

    void wait_until_all_finalized() override {}

    size_t get_num_iterations() const override {
      return n_iterations_;
    }

    void set_num_iterations(size_t n) override {
      progress_->add_work_amount(n - n_iterations_);
      n_iterations_ = n;
    }

    ThreadTask* get_next_task(size_t) override {  // unused
      throw RuntimeError();  // LCOV_EXCL_LINE
    }
};




//------------------------------------------------------------------------------
// MultiThreaded_OrderedJob
//------------------------------------------------------------------------------

class MultiThreaded_OrderedJob : public OrderedJob2 {
  private:
    size_t n_iterations_;
    size_t n_threads_;
    size_t n_tasks_;
    std::vector<uqOrderedTask> tasks_;
    std::vector<OrderedTask2*> assigned_tasks_;

    static constexpr size_t NO_THREAD = size_t(-1);
    static constexpr size_t INVALID_THREAD = size_t(-2);
    progress::work* progress_;
    WaitTask2 wait_task_;
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
    MultiThreaded_OrderedJob(size_t niters, progress::work* progress,
                             std::vector<uqOrderedTask>&& tasks)
      : n_iterations_(niters),
        n_threads_(num_threads_in_team()),
        n_tasks_(tasks.size()),
        tasks_(std::move(tasks)),
        assigned_tasks_(n_tasks_, &wait_task_),
        progress_(progress),
        next_to_start_(0),
        next_to_order_(0),
        next_to_finish_(0),
        ordering_thread_index_(NO_THREAD),
        istart_(0),
        iorder_(0),
        ifinish_(0)
    {
      xassert(n_tasks_ && n_threads_ > 1 && n_iterations_);
      xassert(wait_task_.ready_to_start());
      for (auto& task : tasks_) {
        task->init_parent(this);
      }
    }


    ThreadTask* get_next_task(size_t ith) override {
      if (ith >= n_threads_) return nullptr;
      std::lock_guard<spin_mutex> lock(mutex_);

      OrderedTask2* task = assigned_tasks_[ith];
      task->advance_state();  // finish previously assigned task

      if (ith == ordering_thread_index_) {
        ordering_thread_index_ = NO_THREAD;
        progress_->set_done_amount(next_to_order_);
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


    void abort_execution() override {
      std::lock_guard<spin_mutex> lock(mutex_);
      next_to_start_ = n_iterations_;
      next_to_order_ = n_iterations_;
      next_to_finish_ = n_iterations_;
      ordering_thread_index_ = INVALID_THREAD;
    }


    void wait_until_all_finalized() override {
      // This function is called by the thread which is performing an
      // ORDERING task, which means no new tasks can become
      // READY_TO_FINISH in the meanwhile.
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

    size_t get_num_iterations() const override {
      return n_iterations_;
    }

    void set_num_iterations(size_t n) override {
      std::lock_guard<dt::spin_mutex> lock(mutex_);
      progress_->add_work_amount(n - n_iterations_);
      n_iterations_ = n;
    }
};




//------------------------------------------------------------------------------
// parallel_for_ordered2
//------------------------------------------------------------------------------

void parallel_for_ordered2(size_t niters, NThreads nthreads0,
                           function<uqOrderedTask()> task_factory)
{
  if (!niters) return;
  progress::work progress(niters);
  size_t nthreads = nthreads0.get();

  size_t ntasks = std::min(niters, nthreads * 3/2);
  if (nthreads > ntasks) nthreads = ntasks;

  if (nthreads == 1) {
    SingleThreaded_OrderedJob job(niters, &progress, task_factory());
    job.run();
  }
  else {
    ThreadTeam thteam(nthreads, thpool);
    std::vector<uqOrderedTask> tasks;
    tasks.reserve(ntasks);
    for (size_t i = 0; i < ntasks; ++i) {
      tasks.push_back(task_factory());
    }
    MultiThreaded_OrderedJob job(niters, &progress, std::move(tasks));
    thpool->execute_job(&job);
  }
  progress.done();
}




}  // namespace dt
