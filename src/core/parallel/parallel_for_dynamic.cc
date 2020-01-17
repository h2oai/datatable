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
namespace dt {


//------------------------------------------------------------------------------
// dynamic_task
//------------------------------------------------------------------------------
using dynamicfn_t = std::function<void(size_t)>;

struct alignas(CACHELINE_SIZE) dynamic_task : public thread_task {
  private:
    size_t iter;
    dynamicfn_t fn;
    // PAD_TO_CACHELINE(sizeof(fn) + sizeof(iter) + sizeof(thread_task));

  public:
    dynamic_task() = default;
    dynamic_task(const dynamicfn_t& f);
    dynamic_task(const dynamic_task&);
    dynamic_task& operator=(const dynamic_task&);

    void set_iter(size_t i) noexcept;
    void execute(thread_worker*) override;
};

dynamic_task::dynamic_task(const dynamicfn_t& f)
  : iter(size_t(-1)), fn(f) {}

dynamic_task::dynamic_task(const dynamic_task& other) {
  fn = other.fn;
}

dynamic_task& dynamic_task::operator=(const dynamic_task& other) {
  fn = other.fn;
  return *this;
}


void dynamic_task::set_iter(size_t i) noexcept {
  iter = i;
}

void dynamic_task::execute(thread_worker*) {
  fn(iter);
}




//------------------------------------------------------------------------------
// dynamic_scheduler
//------------------------------------------------------------------------------

class dynamic_scheduler : public thread_scheduler {
  private:
    std::vector<dynamic_task> tasks;
    size_t nthreads;
    size_t num_iterations;
    std::atomic<size_t> iteration_index;

  public:
    dynamic_scheduler(size_t nthreads, size_t niters);
    void set_task(const dynamicfn_t&);
    void set_task(const dynamicfn_t&, size_t i);
    thread_task* get_next_task(size_t thread_index) override;
    void abort_execution() override;
};


dynamic_scheduler::dynamic_scheduler(size_t nthreads_, size_t niters)
  : tasks(nthreads_),
    nthreads(nthreads_),
    num_iterations(niters),
    iteration_index(0) {}


void dynamic_scheduler::set_task(const dynamicfn_t& f) {
  for (size_t i = 0; i < nthreads; ++i)
    tasks[i] = dynamic_task{ f };
}

void dynamic_scheduler::set_task(const dynamicfn_t& f, size_t i) {
  tasks[i] = dynamic_task{ f };
}


thread_task* dynamic_scheduler::get_next_task(size_t thread_index) {
  if (thread_index >= nthreads) return nullptr;
  size_t next_iter = iteration_index.fetch_add(1);
  if (next_iter >= num_iterations) {
    return nullptr;
  }
  dynamic_task* ptask = &tasks[thread_index];
  ptask->set_iter(next_iter);
  return ptask;
}


void dynamic_scheduler::abort_execution() {
  iteration_index.store(num_iterations);
}





//------------------------------------------------------------------------------
// parallel_for_dynamic
//------------------------------------------------------------------------------

void parallel_for_dynamic(size_t nrows, NThreads NThreads_, dynamicfn_t fn) {
  if (nrows == 1) return fn(0);
  size_t ith = dt::this_thread_index();
  size_t nthreads = NThreads_.get();

  // Running from the master thread
  if (!thpool->in_parallel_region()) {
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
    // Cannot change number of threads when in a parallel region
    xassert(nthreads == tt->size());
    auto sch = tt->shared_scheduler<dynamic_scheduler>(nthreads, nrows);
    sch->set_task(fn, ith);
    sch->execute_in_current_thread();
  }
}


void parallel_for_dynamic(size_t nrows, dynamicfn_t fn) {
  parallel_for_dynamic(nrows, NThreads(dt::num_threads_available()), fn);
}


} // namespace dt
