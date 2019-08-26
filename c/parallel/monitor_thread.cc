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
/* enable nice() function in unistd.h */
#define _XOPEN_SOURCE
#include <iostream>
#include <csignal> // std::signal
#include <unistd.h>
#include "parallel/monitor_thread.h"
#include "parallel/thread_worker.h"   // idle_job
#include "progress/progress_manager.h"  // dt::progress::manager
#include "utils/exceptions.h"
#include "parallel/api.h"
namespace dt {


monitor_thread::monitor_thread(idle_job* wc)
  : controller(wc),
    running(true),
    is_active(false)
{
  thread = std::thread(&monitor_thread::run, this);
}


monitor_thread::~monitor_thread() {
  stop_running();
  if (thread.joinable()) thread.join();
}


void monitor_thread::run() noexcept {
  std::signal(SIGINT, sigint_handler);
  constexpr auto SLEEP_TIME = std::chrono::milliseconds(20);
  // Reduce this thread's priority to a minimum.
  // See http://man7.org/linux/man-pages/man2/nice.2.html
  int ret = nice(+19);
  if (ret == -1) {
    std::cout << "[errno " << errno << "] "
              << "when setting nice value of monitor thread\n";
  }
  _set_thread_num(size_t(-1));

  std::unique_lock<std::mutex> lock(mutex);
  while (running) {
    // Sleep state
    while (!is_active && running) {
      sleep_state_cv.wait(lock);
    }

    // Wake state
    while (is_active && running) {
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


void monitor_thread::sigint_handler(int signal) {
  (void) signal;
  progress::manager->set_interrupt();
}


void monitor_thread::set_active(bool a) {
  {
    std::lock_guard<std::mutex> lock(mutex);
    is_active = a;
  }
  sleep_state_cv.notify_one();
}


void monitor_thread::stop_running() {
  std::lock_guard<std::mutex> lock(mutex);
  running = false;
  sleep_state_cv.notify_one();
}



}  // namespace dt
