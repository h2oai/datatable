//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
// Based on shared_mutex.cpp in llvm-libc++, licensed under MIT license (but
// without a copyright notice): https://android.googlesource.com/platform/ndk/
//   +/5de42e6621b3d0131472c3f8838b7f0ccf3e8963/sources/cxx-stl/llvm-libc++/
//   libcxx/include/shared_mutex
//------------------------------------------------------------------------------
#ifndef dt_UTILS_SHARED_MUTEX_h
#define dt_UTILS_SHARED_MUTEX_h
#include <condition_variable>  // std::condition_variable
#include <mutex>               // std::mutex, std::unique_lock
#include "utils/omp.h"

namespace dt {


/**
 * Analogue of `std::shared_mutex` from C++17.
 */
class shared_mutex {
  private:
    unsigned long           state;  // reader count + writer flag
    std::mutex              mutex;  // locked when accessing condition variables
    std::condition_variable wgate;  // condition variable for writers
    std::condition_variable rgate;  // condition variable for readers

    static constexpr unsigned long WRITE_ENTERED
      = 1ULL << (sizeof(unsigned long) * 8 - 1);
    static constexpr unsigned long N_READERS = ~WRITE_ENTERED;

  public:
    shared_mutex() : state(0) {}
    shared_mutex(const shared_mutex&) = delete;
    shared_mutex& operator=(const shared_mutex&) = delete;


    //--------------------------------------------------------------------------
    // Exclusive access
    //--------------------------------------------------------------------------
    void lock() {
      std::unique_lock<std::mutex> lock(mutex);

      // Wait until we can successfully set writer mask
      while (state & WRITE_ENTERED) wgate.wait(lock);

      state |= WRITE_ENTERED;

      // Now wait until all readers have finished reading
      while (state & N_READERS) rgate.wait(lock);
    }

    void unlock() {
      std::unique_lock<std::mutex> lock(mutex);
      state &= ~WRITE_ENTERED;
      wgate.notify_all();
    }


    //--------------------------------------------------------------------------
    // Shared access
    //--------------------------------------------------------------------------
    void lock_shared() {
      std::unique_lock<std::mutex> lock(mutex);
      while (state >= N_READERS) wgate.wait(lock);
      ++state;
    }

    void unlock_shared() {
      std::unique_lock<std::mutex> lock(mutex);
      --state;
      if (state & WRITE_ENTERED) rgate.notify_one();
    }

};


class shared_lock {
  private:
    shared_mutex& mutex;
    bool exclusive;
    uint64_t : 56;

  public:
    shared_lock(shared_mutex& m, bool excl = false)
      : mutex(m), exclusive(excl)
    {
      if (exclusive) {
        mutex.lock();
      } else {
        mutex.lock_shared();
      }
    }

    ~shared_lock() {
      if (exclusive) {
        mutex.unlock();
      } else {
        mutex.unlock_shared();
      }
    }

    void exclusive_start() {
      if (!exclusive) {
        mutex.unlock_shared();
        mutex.lock();
        exclusive = true;
      }
    }

    void exclusive_end() {
      if (exclusive) {
        mutex.unlock();
        mutex.lock_shared();
        exclusive = false;
      }
    }
};


};  // namespace dt
#endif
