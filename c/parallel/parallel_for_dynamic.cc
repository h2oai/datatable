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
#include "parallel/api.h"
#include "parallel/thread_pool.h"
#include "parallel/thread_scheduler.h"
#include "parallel/thread_team.h"
#include "utils/assert.h"
#include "utils/function.h"
#include "utils/macros.h"          // cache_aligned
namespace dt {


//------------------------------------------------------------------------------
// dynamic_task
//------------------------------------------------------------------------------

struct dynamic_task : public thread_task {
  function<void(size_t)> fn;
  size_t iter;

  void execute(thread_worker*) override;
};


void dynamic_task::execute(thread_worker*) {
  fn(iter);
}




//------------------------------------------------------------------------------
// dynamic_scheduler
//------------------------------------------------------------------------------

class dynamic_scheduler : public thread_scheduler {
  private:
    std::vector<cache_aligned<dynamic_task>> tasks;
    size_t num_iterations;
    std::atomic<size_t> iteration_index;

  public:
    dynamic_scheduler(size_t nthreads, size_t niters);
    void set_task(function<void(size_t)>);
    void set_task(function<void(size_t)>, size_t i);
    thread_task* get_next_task(size_t thread_index) override;
    void abort_execution() override;
};


dynamic_scheduler::dynamic_scheduler(size_t nthreads, size_t niters)
  : tasks(nthreads),
    num_iterations(niters),
    iteration_index(0) {}


void dynamic_scheduler::set_task(function<void(size_t)> f) {
  for (auto& task : tasks)
    task.v.fn = f;
}

void dynamic_scheduler::set_task(function<void(size_t)> f, size_t i) {
  tasks[i].v.fn = f;
}


thread_task* dynamic_scheduler::get_next_task(size_t thread_index) {
  size_t next_iter = iteration_index.fetch_add(1);
  if (next_iter >= num_iterations) {
    return nullptr;
  }
  dynamic_task* ptask = &tasks[thread_index].v;
  ptask->iter = next_iter;
  return ptask;
}


void dynamic_scheduler::abort_execution() {
  iteration_index.store(num_iterations);
}




//------------------------------------------------------------------------------
// parallel_for_dynamic
//------------------------------------------------------------------------------

template <typename T>
class thread_shared_ptr {
  static std::mutex mutex;
  static std::atomic<size_t> refcnt;
  static T* instance;

  public:
    template <typename... Args>
    thread_shared_ptr(Args&&... args) {
      if (instance == nullptr) {
        std::lock_guard<std::mutex> lock(mutex);
        if (instance == nullptr) {
          instance = new T(std::forward<Args>(args)...);
        }
      }
      refcnt.fetch_add(1);
    }

    ~thread_shared_ptr() {
      refcnt.fetch_sub(1);
      if (refcnt.load() == 0) {
        delete instance;
        instance = nullptr;
      }
    }

    T* get() const noexcept {
      return instance;
    }
};
template <typename T> T*                  thread_shared_ptr<T>::instance = nullptr;
template <typename T> std::atomic<size_t> thread_shared_ptr<T>::refcnt { 0 };
template <typename T> std::mutex          thread_shared_ptr<T>::mutex;


void parallel_for_dynamic(size_t nrows, function<void(size_t)> fn) {
  thread_pool* thpool = thread_pool::get_instance();
  size_t nthreads = thpool->size();
  size_t ith = dt::this_thread_index();

  // Running from the master thread
  if (ith == size_t(-1)) {
    thread_team tt(nthreads, thpool);
    dynamic_scheduler sch(nthreads, nrows);
    sch.set_task(fn);

    thpool->execute_job(&sch);
  }
  // Running inside a parallel region
  else {
    thread_shared_ptr<dynamic_scheduler> gsch(nthreads, nrows);
    dynamic_scheduler* sch = gsch.get();
    sch->set_task(fn, ith);
    sch->execute_in_current_thread();
  }
}


} // namespace dt
