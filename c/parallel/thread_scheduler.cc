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
#include <atomic>              // std::atomic
#include <condition_variable>  // std::condition_variable
#include <thread>              // std::this_thread
#include "parallel/api.h"
#include "parallel/thread_pool.h"
#include "parallel/thread_scheduler.h"
#include "parallel/thread_worker.h"
#include "utils/assert.h"
namespace dt {



thread_scheduler::~thread_scheduler() {}


void thread_scheduler::abort_execution() {
  // default implementation does nothing
}


void thread_scheduler::execute_in_current_thread() {
  // If this throws an exception, it will propagate to outer level, where the
  // exception will get caught in the outer's level task executor.
  size_t ith = dt::get_thread_num();
  while (true) {
    auto task = get_next_task(ith);
    if (!task) break;
    task->execute(nullptr);
  }
}




//------------------------------------------------------------------------------
// parallel_region
//------------------------------------------------------------------------------

// Implementation class for `dt::parallel_region()` function.
class once_scheduler : public thread_scheduler {
  private:
    std::vector<cache_aligned<size_t>> done;
    thread_task* task;

  public:
    once_scheduler(size_t nthreads, thread_task*);
    thread_task* get_next_task(size_t thread_index) override;
};

once_scheduler::once_scheduler(size_t nth, thread_task* task_)
  : done(nth, 0),
    task(task_) {}

thread_task* once_scheduler::get_next_task(size_t i) {
  if (i >= done.size() || done[i].v) {
    return nullptr;
  }
  done[i].v = 1;
  return task;
}



void parallel_region(function<void()> fn) {
  parallel_region(0, fn);
}

void parallel_region(size_t nthreads, function<void()> fn) {
  thread_pool* thpool = thread_pool::get_instance();
  xassert(thpool->in_master_thread());
  size_t nthreads0 = thpool->size();
  if (nthreads > nthreads0 || nthreads == 0) nthreads = nthreads0;
  simple_task task(fn);
  once_scheduler sch(nthreads, &task);
  thpool->execute_job(&sch);
}




//------------------------------------------------------------------------------
// parallel_for_static
//------------------------------------------------------------------------------

void _parallel_for_static(size_t nrows, size_t min_chunk_size,
                          function<void(size_t, size_t)> fn)
{
  size_t k = nrows / min_chunk_size;

  size_t ith = dt::get_thread_num();

  // Standard parallel loop
  if (ith == size_t(-1)) {
    if (k == 0) {
      fn(0, nrows);
    }
    else {
      size_t nth = get_num_threads();
      size_t chunksize = nrows / k;
      size_t nchunks = nrows / chunksize;

      dt::parallel_region(
        [=] {
          size_t ithread = get_thread_num();
          for (size_t j = ithread; j < nchunks; j += nth) {
            size_t i0 = j * chunksize;
            size_t i1 = i0 + chunksize;
            if (j == nchunks - 1) i1 = nrows;
            fn(i0, i1);
          }
        });
    }
  }
  // Parallel loop within a parallel region
  else {
    if (k == 0) {
      if (ith == 0) fn(0, nrows);
    }
    else {
      size_t nth = get_num_threads();
      size_t chunksize = nrows / k;
      size_t nchunks = nrows / chunksize;

      for (size_t j = ith; j < nchunks; j += nth) {
        size_t i0 = j * chunksize;
        size_t i1 = i0 + chunksize;
        if (j == nchunks - 1) i1 = nrows;
        fn(i0, i1);
      }
    }
  }
}



}  // namespace dt
