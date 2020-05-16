//------------------------------------------------------------------------------
// Copyright 2018-2020 H2O.ai
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
#include "column/rbound.h"
#include "read/output_column.h"
// #include "writebuf.h"
#include "column.h"
namespace dt {
namespace read {


OutputColumn::OutputColumn()
  : nrows_in_chunks_(0),
    na_count_(0),
    stype_(SType::BOOL) {}


OutputColumn::OutputColumn(OutputColumn&& o) noexcept
  : databuf_(std::move(o.databuf_)),
    strbuf_(std::move(o.strbuf_)),
    chunks_(std::move(o.chunks_)),
    nrows_in_chunks_(o.nrows_in_chunks_),
    na_count_(o.na_count_),
    stype_(o.stype_) {}



void* OutputColumn::data_w() {
  return databuf_.xptr();
}


WritableBuffer* OutputColumn::strdata_w() {
  return strbuf_.get();
}


// Call `.archive_data()` before invoking `.to_column()`.
Column OutputColumn::to_column() {
  xassert(!databuf_);
  size_t nchunks = chunks_.size();
  return (nchunks == 0)? Column::new_na_column(0, stype_) :
         (nchunks == 1)? std::move(chunks_[0]) :
                         Column(new Rbound_ColumnImpl(std::move(chunks_)));
}


void OutputColumn::set_stype(SType stype) {
  stype_ = stype;
  for (auto& col : chunks_) {
    col.cast_inplace(stype);
  }
}





}} // namespace dt::read
