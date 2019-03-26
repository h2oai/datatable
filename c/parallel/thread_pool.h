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
#ifndef dt_PARALLEL_THREAD_POOL_h
#define dt_PARALLEL_THREAD_POOL_h
#include <condition_variable>  // std::condition_variable
#include <mutex>               // std::mutex
#include <vector>              // std::vector
#include "parallel/thread_task_scheduler.h"
namespace dt {
using std::size_t;


class task;
class thread_worker;


class thread_pool {
  private:
    std::vector<thread_worker> workers;
    std::mutex sleep_mutex;
    std::condition_variable sleep_alarm;
    // reference to the scheduler while the job is executed (not owned)
    thread_task_scheduler* scheduler;

    thread_task_scheduler* shutdown_scheduler;

  public:
    thread_pool();
    ~thread_pool();

    void execute_job(thread_task_scheduler*);
    void set_number_of_threads(size_t n);
    thread_worker& get_worker(size_t i);

  private:
    task* get_next_task(size_t i);

    friend class thread_worker;
};


// thread_pool is a singleton, returned by this function
thread_pool& get_thread_pool();


}  // namespace dt
#endif
