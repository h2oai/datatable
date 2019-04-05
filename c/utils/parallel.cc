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
#include "parallel/api.h"
#include "utils/exceptions.h"
#include "utils/parallel.h"
#include "options.h"

namespace dt {



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


void ordered_job::execute() {
  constexpr size_t min_nrows_per_thread = 100;
  size_t nthreads = noomp? 0 : nrows / min_nrows_per_thread;
  size_t nchunks = 1 + (nrows - 1)/1000;
  size_t chunksize = 1 + (nrows - 1)/nchunks;

  dt::parallel_for_ordered(
    nchunks,
    nthreads,  // will be truncated to pool size if necessary
    [&](prepare_ordered_fn parallel) {
      ojcptr ctx = start_thread_context();

      parallel(
        [&](size_t j) {
          size_t i0 = j * chunksize;
          size_t i1 = std::min(i0 + chunksize, nrows);
          run(ctx, i0, i1);
        },
        [&](size_t) { order(ctx); },
        nullptr
      );

      finish_thread_context(ctx);
    });
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
