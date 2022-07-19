//------------------------------------------------------------------------------
// Copyright 2018-2022 H2O.ai
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
#include "column/sentinel_str.h"
#include "parallel/api.h"
#include "str/utils.h"
#include "utils/assert.h"
#include "utils/exceptions.h"
#include "options.h"

namespace dt {


//------------------------------------------------------------------------------
// Ordered iteration, produce a string column
//------------------------------------------------------------------------------

class GenStringColumn : public OrderedTask {
  using Buf32 = writable_string_col::buffer_impl<uint32_t>;
  using Buf64 = writable_string_col::buffer_impl<uint64_t>;
  private:
    std::unique_ptr<string_buf> sb_;
    function<void(size_t, string_buf*)> fn_;
    size_t chunk_size_;
    size_t nrows_;

  public:
    GenStringColumn(function<void(size_t, string_buf*)> fn,
                    writable_string_col& outcol, bool force_str64,
                    size_t chunksize, size_t nrows)
      : sb_(force_str64? std::unique_ptr<string_buf>(new Buf64(outcol))
                       : std::unique_ptr<string_buf>(new Buf32(outcol))),
        fn_(fn),
        chunk_size_(chunksize),
        nrows_(nrows) {}

    ~GenStringColumn() override {
      sb_->commit_and_start_new_chunk(nrows_);
    }

    void start(size_t j) override {
      size_t i0 = std::min(j * chunk_size_, nrows_);
      size_t i1 = std::min(i0 + chunk_size_, nrows_);

      sb_->commit_and_start_new_chunk(i0);
      for (size_t i = i0; i < i1; ++i) {
        fn_(i, sb_.get());
      }
    }

    void order(size_t) override {
      sb_->order();
    }
};


Column generate_string_column(function<void(size_t, string_buf*)> fn,
                              size_t nrows,
                              Buffer&& offsets_buffer,
                              bool force_str64,
                              bool force_single_threaded)
{
  if (nrows == 0) {
    return force_str64? Column(new SentinelStr_ColumnImpl<uint64_t>(0))
                      : Column(new SentinelStr_ColumnImpl<uint32_t>(0));
  }
  size_t nchunks = 1 + (nrows - 1)/1000;
  size_t chunksize = 1 + (nrows - 1)/nchunks;

  writable_string_col outcol(std::move(offsets_buffer), nrows, force_str64);

  constexpr size_t min_nrows_per_thread = 100;
  NThreads nthreads = force_single_threaded
    ? NThreads(1)
    : nthreads_from_niters(nchunks, min_nrows_per_thread);

  dt::parallel_for_ordered(nchunks, nthreads,
      [&] {
        return std::make_unique<GenStringColumn>(fn, outcol, force_str64,
                                                 chunksize, nrows);
      });

  return std::move(outcol).to_ocolumn();
}


Column map_str2str(const Column& input_col,
                   dt::function<void(size_t, CString&, string_buf*)> fn)
{
  return generate_string_column(
    [&](size_t i, string_buf* sb) {
      CString& cstr = sb->tmp_str;
      bool isvalid = input_col.get_element(i, &cstr);
      if (!isvalid) cstr.set_na();
      fn(i, cstr, sb);
    },
    input_col.nrows(),
    Buffer(),
    (input_col.stype() == SType::STR64),  // force_str64
    !input_col.allow_parallel_access()    // force_single_threaded
  );
}


}  // namespace dt
