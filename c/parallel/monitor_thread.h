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
#ifndef dt_PARALLEL_MONITOR_THREAD_h
#define dt_PARALLEL_MONITOR_THREAD_h
#include <condition_variable>   // std::condition_variable
#include <cstddef>              // std::size_t
#include <mutex>                // std::mutex
#include <thread>               // std::thread
namespace dt {
using std::size_t;

class idle_job;
void enable_monitor(bool) noexcept;


class monitor_thread {
  friend std::mutex& python_mutex();
  private:
    std::thread thread;
    idle_job* controller;
    std::mutex mutex;
    std::condition_variable sleep_state_cv;
    bool running;
    bool is_active;
    size_t : 48;

  public:
    monitor_thread(idle_job*);
    monitor_thread(const monitor_thread&) = delete;
    monitor_thread(monitor_thread&&) = delete;
    ~monitor_thread();

    /**
     * set_active(true) will cause the thread to awaken and start
     *   running its main job;
     * set_active(false) puts the thread back to sleep.
     */
    void set_active(bool a) noexcept;
    bool get_active();
    void stop_running();

  private:
    void run() noexcept;
    static void sigint_handler(int);
};


class MonitorGuard {
  public:
    MonitorGuard() {
      enable_monitor(true);
    }
    ~MonitorGuard() {
      enable_monitor(false);  // noexcept
    }
};


} // namespace dt
#endif
