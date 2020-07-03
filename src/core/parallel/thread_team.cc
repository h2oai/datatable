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
#include "parallel/api.h"
#include "parallel/thread_pool.h"
#include "parallel/thread_job.h"
#include "parallel/thread_team.h"
#include "utils/exceptions.h"
namespace dt {


thread_team::thread_team(size_t nth, ThreadPool* pool)
  : nthreads(nth),
    thpool(pool),
    nested_scheduler(nullptr),
    barrier_counter {0}
{
  if (thpool->current_team) {
    throw RuntimeError() << "Unable to create a nested thread team";
  }
  thpool->current_team = this;
}


thread_team::~thread_team() {
  thpool->current_team = nullptr;
  auto tmp = nested_scheduler.load();
  delete tmp;
}


size_t thread_team::size() const noexcept {
  return nthreads;
}


void thread_team::wait_at_barrier() {
  size_t n = barrier_counter.fetch_add(1);
  size_t n_target = n - (n % nthreads) + nthreads;
  while (barrier_counter.load() < n_target &&
         !progress::manager->is_interrupt_occurred());
}


void barrier() {
  ThreadPool::get_team_unchecked()->wait_at_barrier();
}


} // namespace dt
