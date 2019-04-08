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
#ifndef dt_PARALLEL_SPIN_MUTEX_h
#define dt_PARALLEL_SPIN_MUTEX_h
#include <atomic>   // std::atomic_flag
namespace dt {


class spin_mutex {
  private:
    std::atomic_flag af = ATOMIC_FLAG_INIT;

  public:
    // spin_mutex() : af() {}

    void lock() {
      // atomic_flag.test_and_set() sets the flag to true and returns the
      // previously held value.
      // If the returned value is `true`, then the lock is currently held by
      // another thread, and setting the flag to true didn't change anything.
      // In this case we will continue looping, until the returned value
      // becomes false.
      // If the returned value is `false`, then the lock was not held by any
      // other thread, and we are safe to exit the loop knowing the lock is
      // currently "owned" by this thread, since we have set `af` to true
      // atomically.
      while (af.test_and_set());
    }

    void unlock() {
      af.clear();
    }
};


}  // namespace dt
#endif
