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
#include "utils/parallel.h"
#include "options.h"
#include "utils/exceptions.h"

namespace dt {


//------------------------------------------------------------------------------
// Order-less iteration
//------------------------------------------------------------------------------

void run_parallel(rangefn run, size_t nrows)
{
  // `min_nrows_per_thread`: avoid processing less than this many rows in each
  // thread, reduce the number of threads if necessary.
  // `min_nrows_per_batch`: the minimum number of rows to process within each
  // thread before sending a progress report signal and checking for
  // interrupts.
  //
  constexpr size_t min_nrows_per_thread = 100;
  constexpr size_t min_nrows_per_batch = 10000;

  if (nrows < min_nrows_per_thread) {
    run(0, nrows, 1);
    // progress.report(nrows);
  }
  else {
    // If the number of rows is too small, then we want to reduce the number of
    // processing threads.
    int nth0 = std::min(config::nthreads,
                        static_cast<int>(nrows / min_nrows_per_thread));
    xassert(nth0 > 0);
    OmpExceptionManager oem;
    #pragma omp parallel num_threads(nth0)
    {
      size_t ith = static_cast<size_t>(omp_get_thread_num());
      size_t nth = static_cast<size_t>(omp_get_num_threads());
      size_t batchsize = min_nrows_per_batch * nth;
      try {
        size_t i = ith;
        do {
          size_t iend = std::min(i + batchsize, nrows);
          run(i, iend, nth);
          i = iend;
          // if (ith == 0) progress.report(iend);
        } while (i < nrows && !oem.stop_requested());
      } catch (...) {
        oem.capture_exception();
      }
    }
    oem.rethrow_exception_if_any();
  }
}



//------------------------------------------------------------------------------
// ordered_job
//------------------------------------------------------------------------------

ordered_job::ordered_job(size_t n, bool force_single_threaded)
  : nrows(n),
    noomp(force_single_threaded) {}

ordered_job::~ordered_job() {}

ojcontext::~ojcontext() {}


ojcptr ordered_job::start_thread_context() {
  return ojcptr();
}

void ordered_job::finish_thread_context(ojcptr& ctx) {
  run(ctx, nrows, nrows);
}


void ordered_job::execute()
{
  constexpr size_t min_nrows_per_thread = 100;

  if (nrows <= min_nrows_per_thread) {
    ojcptr ctx = start_thread_context();
    run(ctx, 0, nrows);
    order(ctx);
    finish_thread_context(ctx);
    // progress.report(nrows);
  }
  else {
    size_t nth0 = std::min(static_cast<size_t>(config::nthreads),
                           nrows / min_nrows_per_thread);
    if (noomp) nth0 = 1;
    (void)nth0;  // Prevent warning about unused variable

    OmpExceptionManager oem;
    #pragma omp parallel num_threads(nth0)
    {
      // int ith = omp_get_thread_num();
      size_t nchunks = 1 + (nrows - 1)/1000;
      size_t chunksize = 1 + (nrows - 1)/nchunks;
      ojcptr ctx;

      try {
        ctx = start_thread_context();
      } catch (...) {
        oem.capture_exception();
      }

      #pragma omp for ordered schedule(dynamic)
      for (size_t j = 0; j < nchunks; ++j) {
        if (oem.stop_requested()) continue;
        size_t i0 = j * chunksize;
        size_t i1 = std::min(i0 + chunksize, nrows);
        try {
          run(ctx, i0, i1);
          // if (ith == 0) progress.report(i1);
        } catch (...) {
          oem.capture_exception();
        }
        #pragma omp ordered
        {
          try {
            order(ctx);
          } catch (...) {
            oem.capture_exception();
          }
        }
      }

      try {
        finish_thread_context(ctx);
      } catch (...) {
        oem.capture_exception();
      }
    }
    oem.rethrow_exception_if_any();
  }
}



//------------------------------------------------------------------------------
// Ordered iteration, produce a string column
//------------------------------------------------------------------------------

class mapper_fw2str : private ordered_job {
  private:
    using iterfn = dt::function<void(size_t, string_buf*)>;
    writable_string_col outcol;
    iterfn f;
    bool str64;
    size_t : 56;

  public:
    mapper_fw2str(iterfn f_, MemoryRange&& offsets_buffer, size_t nrows,
                  bool force_str64, bool force_single_threaded);
    Column* result();

  private:
    class thcontext : public ojcontext {
      public:
        std::unique_ptr<string_buf> sb;
        thcontext(writable_string_col&, bool str64_);
        ~thcontext() override;
    };

    ojcptr start_thread_context() override;
    void run(ojcptr& ctx, size_t i0, size_t i1) override;
    void order(ojcptr& ctx) override;
};


mapper_fw2str::mapper_fw2str(iterfn f_, MemoryRange&& offsets, size_t nrows,
                             bool force_str64, bool force_single_threaded)
  : ordered_job(nrows, force_single_threaded),
    outcol(std::move(offsets), nrows, force_str64),
    f(f_),
    str64(force_str64) {}


mapper_fw2str::thcontext::thcontext(writable_string_col& ws, bool str64_) {
  if (str64_) {
    sb = make_unique<writable_string_col::buffer_impl<uint64_t>>(ws);
  } else {
    sb = make_unique<writable_string_col::buffer_impl<uint32_t>>(ws);
  }
}

mapper_fw2str::thcontext::~thcontext() {}



Column* mapper_fw2str::result() {
  execute();
  return std::move(outcol).to_column();
}


ojcptr mapper_fw2str::start_thread_context() {
  return ojcptr(new thcontext(outcol, str64));
}


void mapper_fw2str::run(ojcptr& ctx, size_t i0, size_t i1) {
  string_buf* sb = static_cast<thcontext*>(ctx.get())->sb.get();
  sb->commit_and_start_new_chunk(i0);
  for (size_t i = i0; i < i1; ++i) {
    f(i, sb);
  }
}


void mapper_fw2str::order(ojcptr& ctx) {
  static_cast<thcontext*>(ctx.get())->sb->order();
}



Column* generate_string_column(dt::function<void(size_t, string_buf*)> fn,
                               size_t n,
                               MemoryRange&& offsets_buffer,
                               bool force_str64,
                               bool force_single_threaded)
{
  mapper_fw2str m(fn, std::move(offsets_buffer), n,
                  force_str64, force_single_threaded);
  return m.result();
}



}  // namespace dt
