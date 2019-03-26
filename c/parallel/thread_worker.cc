//------------------------------------------------------------------------------
// Copyright 2018 H2O.ai
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
#include <thread>   // std::thread
#include "parallel/thread_worker.h"
#include "parallel/thread_pool.h"
namespace dt {


/**
 * The worker creates its own thread of execution, and then detaches it. The
 * thread will be executing function `thread_worker::run()` continuously. The
 * only way to shut down the thread is to cause the `run()` function to stop
 * its loop.
 */
thread_worker::thread_worker(size_t i, thread_pool* tp) {
  thread_index = i;
  thpool = tp;
  std::thread t(&thread_worker::run, this);
  t.detach();
}


/**
 * This is the main function that will be run within the thread. It
 * continuously picks up tasks from the thread pool and executes them. This
 * function exits only when `thpool` becomes `nullptr`. Once this function
 * returns, the thread of execution associated with this worker exits too.
 *
 * Special tasks:
 *   - `shutdown_thread_task` will call `this->shutdown()`, which will cause
 *     the loop to terminate.
 *   - `slumber_task` will
 */
void thread_worker::run() {
  while (thpool) {
    auto task = thpool->get_next_task(thread_index);
    task->execute();
  }
}


void thread_worker::shutdown() {
  thpool = nullptr;
}


}  // namespace dt
