//------------------------------------------------------------------------------
// Copyright 2018 H2O.ai
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
#ifndef dt_PARALLEL_THREAD_TASK_SCHEDULER_h
#define dt_PARALLEL_THREAD_TASK_SCHEDULER_h
#include <memory>      // std::unique_ptr
#include <vector>      // std::vector
namespace dt {
using std::size_t;

// forward-declare
class thread_pool;
class thread_worker;


//------------------------------------------------------------------------------
// task
//------------------------------------------------------------------------------

class task {
  public:
    virtual ~task();
    virtual void execute(thread_worker*) = 0;
};



//------------------------------------------------------------------------------
// Base scheduler
//------------------------------------------------------------------------------

class thread_task_scheduler {
  public:
    virtual ~thread_task_scheduler();
    virtual task* get_next_task(size_t thread_index) = 0;
    virtual void join() = 0;
};



//------------------------------------------------------------------------------
// thread shutdown scheduler
//------------------------------------------------------------------------------

class shutdown_thread_task : public task {
  public:
    void execute(thread_worker*) override;
};

class put_to_sleep_task : public task {
  public:
    thread_task_scheduler* sleep_scheduler;

    void execute(thread_worker*) override;
};


class thread_shutdown_scheduler : public thread_task_scheduler {
  private:
    size_t n_threads_to_keep;
    std::atomic<size_t> n_threads_to_kill;
    shutdown_thread_task shutdown;
    put_to_sleep_task lullaby;

  public:
    void init(size_t nnew, size_t nold, thread_task_scheduler* sch_sleep);
    task* get_next_task(size_t thread_index) override;
    void join() override;
};




//------------------------------------------------------------------------------
// thread sleep scheduler
//------------------------------------------------------------------------------

class thread_sleep_task : public task {
  public:
    std::mutex mutex;
    std::condition_variable alarm;
    thread_task_scheduler* next_scheduler;

    void execute(thread_worker*) override;
};

class thread_sleep_scheduler : public thread_task_scheduler {
  private:
    thread_sleep_task sleep;

  public:
    task* get_next_task(size_t thread_index) override;
    void join() override;
    void awaken(thread_task_scheduler*);
};



}  // namespace dt
#endif
