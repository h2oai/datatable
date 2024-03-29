//------------------------------------------------------------------------------
// Copyright 2019-2023 H2O.ai
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
// Original source taken from:
//
//   https://github.com/preshing/cpp11-on-multicore/blob/master/common/sema.h
//
// which was ditributed under the following license:
//
//   Copyright (c) 2015 Jeff Preshing
//
//   This software is provided 'as-is', without any express or implied
//   warranty. In no event will the authors be held liable for any damages
//   arising from the use of this software.
//
//   Permission is granted to anyone to use this software for any purpose,
//   including commercial applications, and to alter it and redistribute it
//   freely, subject to the following restrictions:
//
//   1. The origin of this software must not be misrepresented; you must not
//      claim that you wrote the original software. If you use this software
//      in a product, an acknowledgement in the product documentation would be
//      appreciated but is not required.
//   2. Altered source versions must be plainly marked as such, and must not be
//      misrepresented as being the original software.
//   3. This notice may not be removed or altered from any source distribution.
//
//------------------------------------------------------------------------------
// This file was modified from the original.
//------------------------------------------------------------------------------
#ifndef dt_PARALLEL_SEMAPHORE_h
#define dt_PARALLEL_SEMAPHORE_h
#include <atomic>
#include <algorithm>
#include <thread>          // std::this_thread
#include "utils/assert.h"
#include "utils/macros.h"

#ifndef DT_SEMAPHORE_SPIN_COUNT
  #define DT_SEMAPHORE_SPIN_COUNT 1000000
#endif

namespace dt {


//------------------------------------------------------------------------------
// Semaphore (Windows)
//------------------------------------------------------------------------------
#if DT_OS_WINDOWS
#include <windows.h>
#undef min
#undef max

class Semaphore {
  private:
    HANDLE m_hSema;

    Semaphore(const Semaphore& other) = delete;
    Semaphore& operator=(const Semaphore& other) = delete;

  public:
    Semaphore() {
      m_hSema = CreateSemaphore(nullptr, 0, MAXLONG, nullptr);
    }

    ~Semaphore() {
      CloseHandle(m_hSema);
    }

    void wait() {
      WaitForSingleObject(m_hSema, INFINITE);
    }

    void signal(int count = 1) {
      ReleaseSemaphore(m_hSema, count, nullptr);
    }
};




//------------------------------------------------------------------------------
// Semaphore (Apple iOS and OSX)
// Can't use POSIX semaphores due to
// http://lists.apple.com/archives/darwin-kernel/2009/Apr/msg00010.html
//------------------------------------------------------------------------------
#elif DT_OS_MACOS
#include <mach/mach.h>

class Semaphore {
  private:
    semaphore_t m_sema;

    Semaphore(const Semaphore& other) = delete;
    Semaphore& operator=(const Semaphore& other) = delete;

  public:
    Semaphore() {
      semaphore_create(mach_task_self(), &m_sema, SYNC_POLICY_FIFO, 0);
    }

    ~Semaphore() {
      semaphore_destroy(mach_task_self(), m_sema);
    }

    void wait() {
      semaphore_wait(m_sema);
    }

    void signal() {
      semaphore_signal(m_sema);
    }

    void signal(int count) {
      while (count-- > 0) {
        semaphore_signal(m_sema);
      }
    }
};




//------------------------------------------------------------------------------
// Semaphore (POSIX, Linux)
//------------------------------------------------------------------------------
#elif (DT_OS_LINUX || DT_OS_FREEBSD)
#include <semaphore.h>

class Semaphore {
  private:
    sem_t m_sema;

    Semaphore(const Semaphore& other) = delete;
    Semaphore& operator=(const Semaphore& other) = delete;

  public:
    Semaphore() {
      sem_init(&m_sema, 0, 0);
    }

    ~Semaphore() {
      sem_destroy(&m_sema);
    }

    void wait() {
      // http://stackoverflow.com/questions/2013181/gdb-causes-sem-wait-to-fail-with-eintr-error
      int rc;
      do {
        rc = sem_wait(&m_sema);
      } while (rc == -1 && errno == EINTR);
    }

    void signal() {
      sem_post(&m_sema);
    }

    void signal(int count) {
      while (count-- > 0) {
        sem_post(&m_sema);
      }
    }
};

#else
  #error Unsupported operating system
#endif



//------------------------------------------------------------------------------
// LightweightSemaphore
//------------------------------------------------------------------------------

class LightweightSemaphore {
  private:
    // m_count: if negative: -(how many threads are waiting in the semaphore's
    //                         queue)
    //          if positive: +(how many threads can be released immediately,
    //                         without having to wait)
    // In addition, any number of threads can be "spin-waiting" at any given
    // time, without modifying the `m_count`. Thus, `m_count` cannot be a
    // reliable indicator of the number of threads currently queued.
    //
    std::atomic<int> m_count;
    Semaphore m_sema;

  public:
    LightweightSemaphore(int initialCount = 0)
      : m_count(initialCount) {}

    bool try_wait() {
      int old = m_count.load(std::memory_order_relaxed);
      return (old > 0) &&
             m_count.compare_exchange_strong(old, old - 1,
                                             std::memory_order_acquire);
    }

    void wait() {
      int spin_count = DT_SEMAPHORE_SPIN_COUNT;
      do {
        if (try_wait()) return;
        // This line significantly boosts the performance.
        // When a thread busy-waits, it consumes its CPU quota, so that when
        // the "real" work arrives, the thread may receive a priority penalty
        // from the OS (especially when the total number of threads is equal
        // to the number of cores in the system).
        #if DT_ARCH_X86_64 && !DT_OS_WINDOWS
          __asm__ volatile(".byte 0xf3,0x90");
        #else
          std::this_thread::yield();
        #endif
      } while (spin_count--);

      int old_count = m_count.fetch_sub(1, std::memory_order_acquire);
      if (old_count <= 0) {
        m_sema.wait();
      }
    }

    void signal(int release_count = 1) {
      int old_count = m_count.fetch_add(release_count,
                                        std::memory_order_release);
      int to_release = std::min(-old_count, release_count);
      if (to_release > 0) {
        m_sema.signal(to_release);
      }
    }
};


} // namespace dt
#endif
