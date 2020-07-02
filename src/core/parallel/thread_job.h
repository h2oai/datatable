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
#ifndef dt_PARALLEL_THREAD_JOB_h
#define dt_PARALLEL_THREAD_JOB_h
#include <cstddef>  // size_t
namespace dt {

class ThreadWorker;
class ThreadTask;


/**
  * Abstract class representing a certain problem that must be solved
  * by the team of thread workers working together.
  *
  * A thread job is subdivided into a sequence of `ThreadTask`s,
  * where each task accomplishes only a small portion of the job, and
  * each task is assigned to a single thread worker.
  *
  */
class ThreadJob {
  public:
    virtual ~ThreadJob();

    // Invoked by a worker (on a worker thread), this method should
    // return the next task to be executed by thread `thread_index`.
    // The lifetime of the returned object should be at least until
    // the next invocation of `get_next_task()` by the thread with
    // the same index.
    virtual ThreadTask* get_next_task(size_t thread_index) = 0;

    // Invoked by `handle_exception()` (and therefore on a worker
    // thread), this method should cancel all pending tasks, or as
    // many as feasible, since their results will not be needed. This
    // call is not supposed to be blocking. The default implementation
    // does nothing (all scheduled tasks continue being executed),
    // which is allowed but suboptimal.
    virtual void abort_execution();
};



class ThreadTask {
  public:
    ThreadTask() = default;
    virtual ~ThreadTask();
    virtual void execute() = 0;
};



}  // namespace dt
#endif
