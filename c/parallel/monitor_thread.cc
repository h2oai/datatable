//------------------------------------------------------------------------------
// Copyright 2019-2020 H2O.ai
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
#include "utils/macros.h"
#if !DT_OS_WINDOWS
  /* enable nice() function in unistd.h */
  #define _XOPEN_SOURCE
  #include <unistd.h>                   // nice
#endif
#include <iostream>
#include <csignal>                      // std::signal, sig_atomic_t
#include "parallel/api.h"
#include "parallel/monitor_thread.h"
#include "progress/progress_manager.h"  // dt::progress::progress_manager
#include "parallel/thread_worker.h"     // idle_job
#include "utils/exceptions.h"

// volatile std::sig_atomic_t gSignalStatus;
using sig_handler_t = void(*)(int);
static sig_handler_t sigint_handler_prev = nullptr;

static volatile std::sig_atomic_t monitor_thread_active = 0;

extern "C" {
  static void sigint_handler(int signal) {
    if (monitor_thread_active) {
      dt::progress::manager->set_interrupt();
    }
    else if (sigint_handler_prev) {
      sigint_handler_prev(signal);
    }
  }
}



namespace dt {


monitor_thread::monitor_thread(idle_job* wc)
  : controller(wc),
    running(true)
{
  monitor_thread_active = 0;
  thread = std::thread(&monitor_thread::run, this);
}


monitor_thread::~monitor_thread() {
  stop_running();
  if (thread.joinable()) thread.join();
}


void monitor_thread::run() noexcept {
  sigint_handler_prev = std::signal(SIGINT, sigint_handler);
  constexpr auto SLEEP_TIME = std::chrono::milliseconds(20);

  // Reduce this thread's priority to a minimum.
  #if DT_OS_WINDOWS
    HANDLE th = thread.native_handle();
    int ret = SetThreadPriority(th, THREAD_PRIORITY_LOWEST);
    if (!ret) {
      std::cout << "[errno " << GetLastError() << "] "
                << "when setting monitor thread priority\r\n";
    }
  #else
    // See http://man7.org/linux/man-pages/man2/nice.2.html
    int ret = nice(+19);
    if (ret == -1) {
      std::cout << "[errno " << errno << "] "
                << "when setting nice value of monitor thread\n";
    }
  #endif

  _set_thread_num(size_t(-1));

  std::unique_lock<std::mutex> lock(mutex);
  while (running) {
    // Sleep state
    while (!monitor_thread_active && running) {
      sleep_state_cv.wait(lock);
    }

    // Wake state
    while (monitor_thread_active && running) {
      try {
        // update_view() should run under the protection of a mutex. This way
        // when the master thread calls `set_active(false)`, it would have to
        // wait until progress manager finishes its communication with the
        // python runtime. Thus, the mutex ensures that after the thread team
        // joins the master thread is free to do any communication with the
        // python without feat that monitor thread might be doing the same at
        // the same time.
        progress::manager->update_view();
      } catch(...) {
        controller->catch_exception();
      }
      sleep_state_cv.wait_for(lock, SLEEP_TIME);
    }
  }
}


void monitor_thread::set_active(bool a) noexcept {
  monitor_thread_active = a;
  sleep_state_cv.notify_one();  // noexcept
}



void monitor_thread::stop_running() {
  std::lock_guard<std::mutex> lock(mutex);
  running = false;
  monitor_thread_active = 0;
  sleep_state_cv.notify_one();
}



}  // namespace dt
