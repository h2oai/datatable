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
#include "ltype.h"
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



void* OutputColumn::data_w(size_t row) const {
  xassert(row >= nrows_in_chunks_);
  return databuf_.xptr((row - nrows_in_chunks_) * stype_elemsize(stype_));
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
  {
    Stats* stats = newcol.stats();
    stats->set_nacount(colinfo_.na_count);
    bool valid = (colinfo_.na_count < nrows_chunk);
    switch (stype_to_ltype(stype_)) {
      case LType::BOOL: {
        auto bstats = dynamic_cast<BooleanStats*>(stats);
        xassert(bstats);
        bstats->set_all_stats(colinfo_.b.count0, colinfo_.b.count1);
        break;
      }
      case LType::INT: {
        stats->set_min(colinfo_.i.min, valid);
        stats->set_max(colinfo_.i.max, valid);
        break;
      }
      case LType::REAL: {
        stats->set_min(colinfo_.f.min, valid);
        stats->set_max(colinfo_.f.max, valid);
        break;
      }
      default: break;
    }
  }
  chunks_.push_back(std::move(newcol));
  reset_colinfo();
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
  xassert(type_bumped_ || !databuf_);
  stype_ = stype;
  reset_colinfo();
}



void OutputColumn::reset_colinfo() {
  colinfo_.na_count = 0;
  switch (stype_) {
    case SType::BOOL: {
      colinfo_.b.count0 = 0;
      colinfo_.b.count1 = 0;
      break;
    }
    case SType::INT8:
    case SType::INT16:
    case SType::INT32:
    case SType::INT64: {
      colinfo_.i.min = std::numeric_limits<int64_t>::max();
      colinfo_.i.max = -std::numeric_limits<int64_t>::max();
      break;
    }
    case SType::FLOAT32:
    case SType::FLOAT64: {
      colinfo_.f.min = std::numeric_limits<double>::infinity();
      colinfo_.f.max = -std::numeric_limits<double>::infinity();
      break;
    }
    case SType::STR32:
    case SType::STR64: break;
    default:
      throw RuntimeError() << "Unexpected stype in fread: " << stype_;
  }
}



void OutputColumn::merge_chunk_stats(const ColInfo& info) {
  colinfo_.na_count += info.na_count;
  switch (stype_) {
    case SType::BOOL: {
      colinfo_.b.count0 += info.b.count0;
      colinfo_.b.count1 += info.b.count1;
      break;
    }
    case SType::INT8:
    case SType::INT16:
    case SType::INT32:
    case SType::INT64: {
      if (info.i.min < colinfo_.i.min) colinfo_.i.min = info.i.min;
      if (info.i.max > colinfo_.i.max) colinfo_.i.max = info.i.max;
      break;
    }
    case SType::FLOAT32:
    case SType::FLOAT64: {
      if (info.f.min < colinfo_.f.min) colinfo_.f.min = info.f.min;
      if (info.f.max > colinfo_.f.max) colinfo_.f.max = info.f.max;
      break;
    }
    case SType::STR32:
    case SType::STR64: break;
    default:
      throw RuntimeError() << "Unexpected stype in fread: " << stype_;
  }
}




}} // namespace dt::read
