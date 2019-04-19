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
#ifndef dt_PARALLEL_THREAD_TEAM_h
#define dt_PARALLEL_THREAD_TEAM_h
#include <atomic>
#include <cstddef>
#include "parallel/thread_pool.h"
namespace dt {
using std::size_t;

// Forward-declare
class thread_scheduler;



class thread_team {
  private:
    size_t nthreads;
    thread_pool* thpool;
    thread_scheduler* nested_scheduler;

    std::atomic<size_t> barrier_counter;

  public:
    thread_team(size_t nth, thread_pool*);
    ~thread_team();

    size_t size() const noexcept;

    template <typename S, typename... Args>
    S* shared_scheduler(Args&&... args) {
      if (!nested_scheduler) {
        std::lock_guard<std::mutex> lock(thpool->global_mutex);
        if (!nested_scheduler) {
          nested_scheduler = new S(std::forward<Args>(args)...);
        }
      }
      return reinterpret_cast<S*>(nested_scheduler);
    }

    void wait_at_barrier();
};


} // namespace dt
#endif
