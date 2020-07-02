//------------------------------------------------------------------------------
// Copyright 2019-2020 H2O.ai
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
//------------------------------------------------------------------------------
#ifndef dt_PARALLEL_THREAD_WORKER_h
#define dt_PARALLEL_THREAD_WORKER_h
#include <atomic>               // std::atomic
#include <condition_variable>   // std::condition_variable
#include <cstddef>              // std::size_t
#include <memory>               // std::unique_ptr
#include <mutex>                // std::mutex
#include <thread>               // std::thread
#include "parallel/monitor_thread.h"
#include "parallel/semaphore.h"
#include "parallel/thread_job.h"
namespace dt {

// Forward-declare
class Job_Idle;


/**
 * A class that encapsulates thread-specific runtime information. After
 * instantiation, we expect this class to be accessed within its own thread
 * only. This makes it safe to have variables such as `scheduler` non-atomic.
 *
 * Any communication with the worker (including changing to a new scheduler)
 * is performed only via the current scheduler: the scheduler may emit a task
 * that changes the worker's state.
 *
 * The thread stops running when `scheduler` becomes nullptr.
 */
class ThreadWorker {
  private:
    const size_t thread_index;
    std::thread  thread;
    ThreadJob*  scheduler;
    Job_Idle* controller;

  public:
    ThreadWorker(size_t i, Job_Idle*);
    ThreadWorker(const ThreadWorker&) = delete;
    ThreadWorker(ThreadWorker&&) = delete;
    ~ThreadWorker();

    void run() noexcept;
    void run_master(ThreadJob*) noexcept;
    size_t get_index() const noexcept;
    void assign_job(ThreadJob*) noexcept;
};




}  // namespace dt
#endif
