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


/**
 * Shared "busy" mutex implementation with `while` loops
 * instead of `std::mutex`. Could be useful when there are frequent, but short
 * read operations.
 */
class shared_bmutex {
  private:
    size_t state; // reader count + writer flag
    static constexpr size_t WRITE_ENTERED = 1ULL << (sizeof(size_t) * 8 - 1);

  public:
    shared_bmutex() : state(0) {}


    //--------------------------------------------------------------------------
    // Exclusive access
    //--------------------------------------------------------------------------
    void lock() {
      size_t state_old;
      // Needed for the case when there are multiple threads waiting for the
      // exclusive access.
      bool exclusive_request = false;

      while (true) {

        #pragma omp atomic capture
        {state_old = state; state |= WRITE_ENTERED;}

        if ((state_old & WRITE_ENTERED) && !exclusive_request) continue;
        if (state_old == WRITE_ENTERED) break;
        exclusive_request = true;
      }
    }


    void unlock() {
      // Clear the writer bit. Note, one can not simply set `state` to `0`,
      // because there may be some readers waiting in the `shared_lock()`.
      // These readers increment and decrement the `state`, so setting
      // it to `0` may result in the incorrect mutex behavior.
      #pragma omp atomic update
      state &= ~WRITE_ENTERED;
    }


    //--------------------------------------------------------------------------
    // Shared access
    //--------------------------------------------------------------------------
    void lock_shared() {
      size_t state_old;

      while (true) {
        #pragma omp atomic read
        state_old = state;

        // This check is required to prevent deadlocks, so that exclusive `lock()`
        // takes priority over the `lock_shared()` when competing. Note, that
        // the `state` should be "omp atomic read", otherwise behavior is unpredictable
        // on some systems.
        if (state_old & WRITE_ENTERED) continue;

        #pragma omp atomic capture
        state_old = state++;

        if (state_old & WRITE_ENTERED) {
          #pragma omp atomic update
          --state;
        } else break;
      }
    }


    void unlock_shared() {
      #pragma omp atomic update
      --state;
    }

};


template<class T> class shared_lock {
  private:
    T& mutex;
    bool exclusive;
    uint64_t : 56;

  public:
    shared_lock(T& m, bool excl = false)
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
