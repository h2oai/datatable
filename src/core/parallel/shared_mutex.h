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
#ifndef dt_PARALLEL_SHARED_MUTEX_h
#define dt_PARALLEL_SHARED_MUTEX_h
#include <atomic>              // std::atomic
#include <condition_variable>  // std::condition_variable
#include <mutex>               // std::mutex, std::unique_lock

namespace dt {


//------------------------------------------------------------------------------
// shared_mutex
//------------------------------------------------------------------------------

/**
 * Equivalent of `std::shared_mutex` from C++17. Also known as read-write mutex.
 * This mutex allows to kinds of locking: for read-only access, and for writing.
 * Multiple threads can hold read lock at the same time, whereas writing is
 * exclusive: only one thread can hold this lock, and no other threads can have
 * even read access at the same time.
 */
class shared_mutex {
  private:
    size_t                  state;  // reader count + writer flag
    std::mutex              mutex;  // locked when accessing condition variables
    std::condition_variable wgate;  // condition variable for writer
    std::condition_variable rgate;  // condition variable for readers

    static constexpr size_t WRITE_ENTERED = 1ULL << (sizeof(size_t) * 8 - 1);

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
      while (state & ~WRITE_ENTERED) rgate.wait(lock);
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
      while (state >= ~WRITE_ENTERED) wgate.wait(lock);
      ++state;
    }

    void unlock_shared() {
      std::unique_lock<std::mutex> lock(mutex);
      --state;
      if (state & WRITE_ENTERED) rgate.notify_one();
    }

};




//------------------------------------------------------------------------------
// shared_bmutex
//------------------------------------------------------------------------------

/**
 * Shared "busy" mutex implementation with `while` loops instead of
 * `std::mutex`. Could be useful when there are frequent, but short read
 * operations.
 */
class shared_bmutex {
  private:
    // `state` variable is the indicator of the locked state of the mutex.
    // If the mutex is unlocked, the `state` is 0.
    // If the mutex is locked for exclusive access, the `WRITE_ENTERED` bit
    // will be set on the `state`. While the bit is set, no thread can acquire
    // either exclusive or regular lock on the mutex.
    // The remaining bits of `state` count how many threads has requested a
    // "reader" access to the mutex. We increment `state` when a shared lock is
    // requested, and decrement when the shared lock is released.
    //
    // Externally, if the `WRITE_ENTERED` bit is set on the `state`, then the
    // remaining bits are 0. Internally, this invariant may be temporarily
    // broken as the locks are acquired. Specifically, during exclusive locking
    // the `WRITE_ENTERED` flag is set, and then the lock waits until all reader
    // threads finish their reading. During read-only locking, the `state` may
    // be temporarily incremented while `WRITE_ENTERED` is set, but then we
    // immediately decrement it again and wait for exclusive flag to clear.
    //
    std::atomic<size_t> state;
    static constexpr size_t WRITE_ENTERED = 1ULL << (sizeof(size_t) * 8 - 1);

  public:
    shared_bmutex() : state(0) {}


    //--------------------------------------------------------------------------
    // Exclusive access
    //--------------------------------------------------------------------------
    void lock() {
      // Wait until no other thread is having exclusive access; i.e. wait until
      // the `state` has bit WRITE_ENTERED clear. Once this is true, set the bit
      // WRITE_ENTERED, claiming exclusive access for the current thread.
      // Note: `state.compare_exchanege_weak(exp, newval)` will check whether
      // `state==exp`, and if yes then write `newval` into `state`; and if no
      // the variable `exp` will be updated with the latest value of `state`.
      // The return value is `true` if the `state` was updated, and `false`
      // otherwise.
      size_t state_old = 0;
      while (!state.compare_exchange_weak(state_old, state_old|WRITE_ENTERED,
                                          std::memory_order_relaxed)) {
        state_old &= ~WRITE_ENTERED;
      }

      // Now wait until all pending readers finished reading
      while (state.load(std::memory_order_acquire) != WRITE_ENTERED);
    }


    void unlock() {
      // Clear the writer bit. Note, one can not simply set `state` to `0`,
      // because there may be some readers waiting in the `lock_shared()`.
      // These readers increment and decrement the `state`, so setting
      // it to `0` may result in the incorrect mutex behavior.
      state.fetch_and(~WRITE_ENTERED, std::memory_order_release);
    }


    //--------------------------------------------------------------------------
    // Shared access
    //--------------------------------------------------------------------------
    void lock_shared() {
      while (true) {
        // If the mutex was locked for exclusive access, wait until the writer's
        // bit is cleared.
        while (state.load(std::memory_order_relaxed) & WRITE_ENTERED);

        // Increment the state, to indicate that we acquire a reader's lock.
        size_t state_old = state.fetch_add(1, std::memory_order_acquire);

        // If during the previous operation the `state` had no exclusive bit
        // set, then the lock was acquired successfully, and we can return.
        if (!(state_old & WRITE_ENTERED)) return;

        // Otherwise, we relinquish the lock, and wait again until the end of
        // exclusive access.
        state.fetch_sub(1, std::memory_order_relaxed);
      }
      // Note that the pattern of `state++; [check WRITE_ENTERED]; state--` can
      // result in a livelock with the exclusive lock, which waits until all
      // low bits of `state` become 0.
      // In practice the chances of this happening should be exceedingly small,
      // and even if it does the livelock would resolve itself after several
      // wasted cycles.
    }


    void unlock_shared() {
      state.fetch_sub(1, std::memory_order_release);
    }

};




//------------------------------------------------------------------------------
// shared_lock
//------------------------------------------------------------------------------

/**
 * Lock class that can accept either a `shared_mutex`, or `shared_bmutex`.
 *
 * Usage:
 *
 *     // mutex variable must previously be declared and accessible from
 *     // multiple threads
 *     dt::shared_mutex shmutex;
 *     ...
 *
 *     { // Acquire read-only lock
 *       dt::shared_lock< dt::shared_mutex> lock(shmutex, false);
 *     }
 *
 *     { // Acquire exclusive lock
 *       dt::shared_lock< dt::shared_mutex> lock(shmutex, true);
 *     }
 */
template <typename T>
class shared_lock {
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
