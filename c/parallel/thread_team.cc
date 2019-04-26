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
#include "utils/exceptions.h"
namespace dt {


thread_team::thread_team(size_t nth, thread_pool* pool)
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
  while (barrier_counter.load() < n_target);
}


void barrier() {
  thread_pool::get_team_unchecked()->wait_at_barrier();
}


} // namespace dt
