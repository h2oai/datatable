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
#include "parallel/thread_job.h"
#include "parallel/thread_team.h"
#include "utils/assert.h"
#include "utils/macros.h"          // cache_aligned
namespace dt {



//------------------------------------------------------------------------------
// simple_task
//------------------------------------------------------------------------------

class simple_task : public ThreadTask {
  private:
    function<void()> f;
  public:
    simple_task(function<void()>);
    void execute() override;
};


simple_task::simple_task(function<void()> f_) : f(f_) {}

void simple_task::execute() {
  f();
}



//------------------------------------------------------------------------------
// once_scheduler
//------------------------------------------------------------------------------

// Implementation class for `dt::parallel_region()` function.
class once_scheduler : public ThreadJob {
  private:
    std::vector<cache_aligned<size_t>> done;
    ThreadTask* task;

  public:
    once_scheduler(size_t nthreads, ThreadTask*);
    ThreadTask* get_next_task(size_t thread_index) override;
};

once_scheduler::once_scheduler(size_t nth, ThreadTask* task_)
  : done(nth, 0),
    task(task_) {}

ThreadTask* once_scheduler::get_next_task(size_t i) {
  if (i >= done.size() || done[i].v) {
    return nullptr;
  }
  done[i].v = 1;
  return task;
}




//------------------------------------------------------------------------------
// parallel_region
//------------------------------------------------------------------------------

void parallel_region(function<void()> fn) {
  parallel_region(NThreads(), fn);
}

void parallel_region(NThreads nthreads_, function<void()> fn) {
  xassert(!thpool->in_parallel_region());
  size_t nthreads = nthreads_.get();
  ThreadTeam tt(nthreads, thpool);

  simple_task task(fn);
  once_scheduler sch(nthreads, &task);
  thpool->execute_job(&sch);
}




}  // namespace dt
