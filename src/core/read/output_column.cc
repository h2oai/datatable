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
#include "utils/temporary_file.h"
#include "column.h"
#include "stype.h"
namespace dt {
namespace read {


OutputColumn::OutputColumn()
  : nrows_in_chunks_(0),
    stype_(SType::BOOL),
    type_bumped_(false),
    present_in_buffer_(true) {}


OutputColumn::OutputColumn(OutputColumn&& o) noexcept
  : databuf_(std::move(o.databuf_)),
    strbuf_(std::move(o.strbuf_)),
    chunks_(std::move(o.chunks_)),
    nrows_in_chunks_(o.nrows_in_chunks_),
    stype_(o.stype_),
    type_bumped_(o.type_bumped_),
    present_in_buffer_(o.present_in_buffer_) {}



void* OutputColumn::data_w(size_t row) {
  xassert(row >= nrows_in_chunks_);
  return static_cast<char*>(databuf_.xptr())
         + (row - nrows_in_chunks_) * stype_elemsize(stype_);
}


MemoryWritableBuffer* OutputColumn::strdata_w() {
  return strbuf_.get();
}


void OutputColumn::archive_data(size_t nrows_written,
                                std::shared_ptr<TemporaryFile>& tempfile)
{
  if (nrows_written == nrows_in_chunks_) return;
  if (type_bumped_ || !present_in_buffer_) return;
  xassert(nrows_written > nrows_in_chunks_);

  size_t is_string = (stype_ == SType::STR32 || stype_ == SType::STR64);
  size_t elemsize = stype_elemsize(stype_);

  size_t nrows_chunk = nrows_written - nrows_in_chunks_;
  size_t data_size = elemsize * (nrows_chunk + is_string);
  Buffer stored_databuf, stored_strbuf;
  if (tempfile) {
    auto writebuf = tempfile->data_w();
    {
      Buffer tmpbuf;
      tmpbuf.swap(databuf_);
      size_t offset = writebuf->write(data_size, tmpbuf.rptr());
      stored_databuf = Buffer::tmp(tempfile, offset, data_size);
    }
    if (is_string) {
      strbuf_->finalize();
      Buffer tmpbuf = strbuf_->get_mbuf();
      size_t offset = writebuf->write(tmpbuf.size(), tmpbuf.rptr());
      stored_strbuf = Buffer::tmp(tempfile, offset, tmpbuf.size());
      strbuf_ = nullptr;
    }
  }
  else {
    stored_databuf.swap(databuf_);
    stored_databuf.resize(data_size);
    if (is_string) {
      strbuf_->finalize();
      stored_strbuf = strbuf_->get_mbuf();
      strbuf_ = nullptr;
    }
  }

  Column newcol = is_string? Column::new_string_column(nrows_chunk,
                                                       std::move(stored_databuf),
                                                       std::move(stored_strbuf))
                           : Column::new_mbuf_column(nrows_chunk, stype_,
                                                     std::move(stored_databuf));
  newcol.stats()->set_nacount(colinfo_.na_count);
  chunks_.push_back(std::move(newcol));
  colinfo_.na_count = 0;
  nrows_in_chunks_ = nrows_written;
  xassert(!databuf_ && !strbuf_);
}


void OutputColumn::allocate(size_t new_nrows) {
  if (type_bumped_ || !present_in_buffer_) return;
  xassert(new_nrows >= nrows_in_chunks_);

  size_t is_string = (stype_ == SType::STR32 || stype_ == SType::STR64);
  size_t elemsize = stype_elemsize(stype_);
  size_t new_nrows_allocated = new_nrows - nrows_in_chunks_;
  size_t allocsize = (new_nrows_allocated + is_string) * elemsize;
  databuf_.resize(allocsize);

  if (is_string) {
    size_t zero = 0;
    std::memcpy(databuf_.xptr(), &zero, elemsize);
    if (!strbuf_) {
      strbuf_ = std::unique_ptr<MemoryWritableBuffer>(
                    new MemoryWritableBuffer(allocsize));
    }
  }
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
  if (nrows_in_chunks_) type_bumped_ = true;
}


void OutputColumn::add_na_count(size_t n) {
  colinfo_.na_count += n;
}



}} // namespace dt::read
