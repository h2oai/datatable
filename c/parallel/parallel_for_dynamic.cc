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
#include <atomic>     // std::atomic
#include <vector>     // std::vector
#include "parallel/api.h"
#include "parallel/thread_pool.h"
#include "parallel/thread_scheduler.h"
#include "parallel/thread_team.h"
#include "utils/assert.h"
#include "utils/function.h"
#include "utils/macros.h"          // cache_aligned
#include <iostream>
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
    size_t nthreads;
    size_t num_iterations;
    std::atomic<size_t> iteration_index;

  public:
    dynamic_scheduler(size_t nthreads, size_t niters);
    void set_task(function<void(size_t)>);
    void set_task(function<void(size_t)>, size_t i);
    thread_task* get_next_task(size_t thread_index) override;
    void abort_execution() override;
};


dynamic_scheduler::dynamic_scheduler(size_t nthreads_, size_t niters)
  : tasks(nthreads_),
    nthreads(nthreads_),
    num_iterations(niters),
    iteration_index(0) {}


void dynamic_scheduler::set_task(function<void(size_t)> f) {
  for (size_t i = 0; i < nthreads; ++i)
    tasks[i].v.fn = f;
}

void dynamic_scheduler::set_task(function<void(size_t)> f, size_t i) {
  tasks[i].v.fn = f;
}


thread_task* dynamic_scheduler::get_next_task(size_t thread_index) {
  if (thread_index >= nthreads) return nullptr;
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

void parallel_for_dynamic(size_t nrows, size_t nthreads,
                           function<void(size_t)> fn)
{
  size_t ith = dt::this_thread_index();

  // Running from the master thread
  if (ith == size_t(-1)) {
    thread_pool* thpool = thread_pool::get_instance_unchecked();

    size_t tp_size = thpool->size();
    if (nthreads == 0) nthreads = tp_size;
    size_t tt_size = std::min(nthreads, tp_size);
    thread_team tt(tt_size, thpool);
    dynamic_scheduler sch(tt_size, nrows);
    sch.set_task(fn);

    thpool->execute_job(&sch);
  }
  // Running inside a parallel region
  else {
    thread_team* tt = thread_pool::get_team_unchecked();
    size_t tt_size = tt->size();
    // Cannot change numnber of threads, if in a parallel region
    xassert(nthreads == tt_size);
    auto sch = tt->shared_scheduler<dynamic_scheduler>(tt_size, nrows);
    sch->set_task(fn, ith);
    sch->execute_in_current_thread();
  }
}


void parallel_for_dynamic(size_t nrows, function<void(size_t)> fn) {
  parallel_for_dynamic(nrows, dt::num_threads_available(), fn);
}


} // namespace dt
