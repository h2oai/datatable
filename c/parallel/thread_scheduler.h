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
#ifndef dt_PARALLEL_THREAD_SCHEDULER_h
#define dt_PARALLEL_THREAD_SCHEDULER_h
#include <memory>      // std::unique_ptr
#include <vector>      // std::vector
namespace dt {
using std::size_t;

// forward-declare
class thread_pool;
class thread_worker;


//------------------------------------------------------------------------------
// thread_task
//------------------------------------------------------------------------------

class thread_task {
  public:
    virtual ~thread_task();
    virtual void execute(thread_worker*) = 0;
};



//------------------------------------------------------------------------------
// Base scheduler
//------------------------------------------------------------------------------

class thread_scheduler {
  private:
    std::mutex mtx;
    std::exception_ptr saved_exception;

  public:
    //--------------------------------------------------------------------------
    // Public API
    //--------------------------------------------------------------------------

    // Called from the master thread, this function will block until all the
    // work is finished and all worker threads have been put to sleep. If there
    // was an exception during execution of any of the tasks, this exception
    // will be rethrown here (but only after all workers were put to sleep).
    void join();

    // Called from worker threads, within the `catch(...){ }` block, this method
    // is used to signal that an exception have occurred. The method will save
    // this exception and then call `abort_execution()` (see below). The saved
    // exception will be re-thrown at the end of `join()`.
    void handle_exception() noexcept;


    //--------------------------------------------------------------------------
    // Subclass API
    //--------------------------------------------------------------------------

    virtual ~thread_scheduler();

    // Invoked by a worker (on a worker thread), this method should return the
    // next task to be executed by thread `thread_index`. The returned pointer's
    // lifetime should be at least until the next invocation of
    // `get_next_task()` by the thread with the same index.
    virtual thread_task* get_next_task(size_t thread_index) = 0;

    // Invoked by `join()`, this function should wait and return only when all
    // scheduled tasks were completed. At the end of this function all worker
    // threads must be on the "sleep" schedule.
    virtual void wait_until_finish() = 0;

    // Invoked by `handle_exception()` (and therefore on a worker thread), this
    // method should cancel all pending tasks, or as many as feasible, since
    // their results will not be needed. This call is not supposed to be
    // blocking. The default implementation does nothing (all scheduled tasks
    // continue being executed), which is allowed but sub-optimal.
    virtual void abort_execution();
};



//------------------------------------------------------------------------------
// thread shutdown scheduler
//------------------------------------------------------------------------------

class shutdown_thread_task : public thread_task {
  public:
    void execute(thread_worker*) override;
};

class put_to_sleep_task : public thread_task {
  public:
    thread_scheduler* sleep_scheduler;

    void execute(thread_worker*) override;
};


class thread_shutdown_scheduler : public thread_scheduler {
  private:
    size_t n_threads_to_keep;
    std::atomic<size_t> n_threads_to_kill;
    shutdown_thread_task shutdown;
    put_to_sleep_task lullaby;

  public:
    void init(size_t nnew, size_t nold, thread_scheduler* sch_sleep);
    thread_task* get_next_task(size_t thread_index) override;
    void wait_until_finish() override;
};




//------------------------------------------------------------------------------
// thread sleep scheduler
//------------------------------------------------------------------------------

class thread_sleep_task : public thread_task {
  public:
    std::mutex mutex;
    std::condition_variable alarm;
    thread_scheduler* next_scheduler;

    void execute(thread_worker*) override;
};

class thread_sleep_scheduler : public thread_scheduler {
  private:
    thread_sleep_task sleep;

  public:
    thread_task* get_next_task(size_t thread_index) override;
    void wait_until_finish() override;
    void awaken(thread_scheduler*);
};



}  // namespace dt
#endif
