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
#ifndef dt_PARALLEL_JOB_SHUTDOWN_h
#define dt_PARALLEL_JOB_SHUTDOWN_h
#include "parallel/thread_job.h"
#include "parallel/job_idle.h"
namespace dt {



/**
  * This ThreadJob is used to resize the thread pool by shutting down
  * some of the existing threads.
  */
class Job_Shutdown : public ThreadJob {
  private:
    class ShutdownTask : public ThreadTask {
      public:
        void execute() override;
    };

    size_t       n_threads_to_keep_;
    Job_Idle*    controller_;
    ShutdownTask shutdown_task_;

  public:
    Job_Shutdown(size_t nnew, Job_Idle*);
    ThreadTask* get_next_task(size_t thread_index) override;
};




}  // namespace dt
#endif
