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
#include "utils/assert.h"
#include "utils/function.h"
namespace dt {


//------------------------------------------------------------------------------
// dynamic_scheduler
//------------------------------------------------------------------------------

class dynamic_scheduler : public thread_scheduler {
  private:
    struct dynamic_task : public thread_task {
      function<void(size_t)> fn;
      size_t iter;
      void execute(thread_worker*) override;
    };

    std::vector<cache_aligned<dynamic_task>> tasks;
    size_t num_iterations;
    std::atomic<size_t> iteration_index;

  public:
    dynamic_scheduler(size_t nthreads, size_t niters);
    void set_task(function<void(size_t)>);
    thread_task* get_next_task(size_t thread_index) override;
};


void dynamic_scheduler::dynamic_task::execute(thread_worker*) {
  fn(iter);
}


dynamic_scheduler::dynamic_scheduler(size_t nthreads, size_t niters)
  : tasks(nthreads),
    num_iterations(niters),
    iteration_index(0) {}


void dynamic_scheduler::set_task(function<void(size_t)> f) {
  for (auto& task : tasks)
    task.v.fn = f;
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




//------------------------------------------------------------------------------
// parallel_for_dynamic
//------------------------------------------------------------------------------

void parallel_for_dynamic(size_t nrows, function<void(size_t)> fn) {
  thread_pool* thpool = thread_pool::get_instance();
  xassert(thpool->in_master_thread());
  size_t nthreads = thpool->size();

  dynamic_scheduler sch(nthreads, nrows);
  sch.set_task(fn);
  thpool->execute_job(&sch);
}


} // namespace dt
