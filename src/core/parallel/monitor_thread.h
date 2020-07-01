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
#ifndef dt_PARALLEL_MONITOR_THREAD_h
#define dt_PARALLEL_MONITOR_THREAD_h
#include <condition_variable>   // std::condition_variable
#include <cstddef>              // std::size_t
#include <mutex>                // std::mutex
#include <thread>               // std::thread
namespace dt {

class idle_job;
void enable_monitor(bool) noexcept;


/**
  * This class represents a monitor thread, i.e. a low-priority
  * background thread that periodically checks task progress and
  * interrupt status, and sends status updates to python.
  *
  * There can only be one monitor_thread instance at a time, since
  * at least part of the monitor_thread's state is kept in static
  * variables that are also accessed from a signal handler.
  */
class monitor_thread {
  private:
    std::thread             thread_;
    idle_job*               controller_;
    std::condition_variable_any sleep_state_cv_;
    bool                    running_;
    size_t : 56;

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

  private:
    void run() noexcept;
    void stop_running();
};


class MonitorGuard {
  public:
    MonitorGuard();
    ~MonitorGuard();
};




} // namespace dt
#endif
