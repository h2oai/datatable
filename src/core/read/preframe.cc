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
#include "csv/reader.h"              // GenericReader
#include "csv/reader_parsers.h"
#include "read/preframe.h"
#include "utils/temporary_file.h"    // TemporaryFile
#include "column.h"
#include "datatable.h"
namespace dt {
namespace read {



PreFrame::PreFrame(const GenericReader* reader) noexcept
  : g_(reader),
    nrows_allocated_(0),
    nrows_written_(0) {}



//------------------------------------------------------------------------------
// Columns
//------------------------------------------------------------------------------

size_t PreFrame::ncols() const noexcept {
  return columns_.size();
}


void PreFrame::set_ncols(size_t ncols) {
  xassert(ncols >= columns_.size());
  xassert(nrows_written_ == 0);  // TODO: handle the case when this isn't true
  columns_.resize(ncols);
}




//------------------------------------------------------------------------------
// Rows
//------------------------------------------------------------------------------

size_t PreFrame::nrows_allocated() const noexcept {
  return nrows_allocated_;
}

size_t PreFrame::nrows_written() const noexcept {
  return nrows_written_;
}


void PreFrame::preallocate(size_t nrows) {
  xassert(nrows_written_ == 0);
  size_t memory_limit = g_->memory_bound;

  // Possibly reduce memory allocation if there is a memory limit
  if (memory_limit != MEMORY_UNLIMITED) {
    size_t row_size = 0;
    for (const auto& col : columns_) {
      row_size += col.elemsize() * (1 + col.is_string());
    }
    if (row_size * nrows > memory_limit) {
      nrows = std::max(size_t(1), memory_limit / row_size);
      if (g_->verbose) {
        g_->trace("Allocation size reduced to %zu rows due to memory_bound "
                  "parameter", nrows);
      }
    }
  }
  for (auto& col : columns_) {
    col.allocate(nrows);
  }
  nrows_allocated_ = nrows;
}


/**
  * Make sure there is enough room in the columns to write
  * `nrows_in_chunk` rows. This parameter is passed by reference,
  * because occasionally PreFrame will need to reduce the effective
  * number of rows in the chunk (this happens when the total number)
  * of rows exceeds `max_nrows_`.
  *
  * The `ordered_loop` variable allows us to retrieve information
  * about the current state of iteration, and to wait until the
  * currently pending data is safely written if we need to reallocate
  * buffers.
  *
  * This function will also adjust the `nrows_written` counter, and
  * thus should be called from the ordered section only.
  */
void PreFrame::ensure_output_nrows(size_t& nrows_in_chunk, size_t ichunk,
                                   dt::ordered* ordered_loop)
{
  size_t nrows_new = nrows_written_ + nrows_in_chunk;
  size_t nrows_max = g_->max_nrows;
  size_t memory_limit = g_->memory_bound;

  // The loop is run at most once. In the most common case it doesn't
  // run at all.
  while (nrows_new > nrows_allocated_) {
    // If the new number of rows would exceed `nrows_max`, then no need
    // to reallocate the output, just truncate the rows in the current
    // chunk.
    if (nrows_new > nrows_max) {
      xassert(nrows_written_ <= nrows_max);
      nrows_in_chunk = nrows_max - nrows_written_;
      nrows_new = nrows_max;
      if (nrows_new <= nrows_allocated_) break;
    }

    // Estimate the final number of rows that will be needed
    size_t nchunks = ordered_loop->get_n_iterations();
    xassert(ichunk < nchunks);
    if (ichunk < nchunks - 1) {
      nrows_new = std::min(
          nrows_max,
          std::max(
              1024 + nrows_allocated_,
              static_cast<size_t>(1.2 * nrows_new * nchunks / (ichunk + 1))
      ));
    }

    xassert(nrows_new >= nrows_in_chunk + nrows_written_);
    ordered_loop->wait_until_all_finalized();
    archive_column_chunks(nrows_new);

    // If there is a memory limit, then we potentially need to adjust
    // the number of rows allocated, so as not to exceed this limit.
    if (memory_limit != MEMORY_UNLIMITED) {
      size_t nrows_extra = nrows_new - nrows_written_;
      size_t archived_size = 0;
      for (const auto& col : columns_) {
        archived_size += col.archived_size();
      }
      double avg_size_per_row = 1.0 * archived_size / nrows_written_;
      if (nrows_extra * avg_size_per_row > memory_limit) {
        nrows_extra = std::max(
            nrows_in_chunk,
            static_cast<size_t>(memory_limit / avg_size_per_row)
        );
        nrows_new = nrows_written_ + nrows_extra;
      }
    }

    g_->trace("Too few rows allocated, reallocating to %zu rows", nrows_new);

    // Now reallocate all columns for a proper number of rows
    for (auto& col : columns_) {
      col.allocate(nrows_new);
    }
    nrows_allocated_ = nrows_new;
  }

  if (nrows_new == nrows_max) {
    ordered_loop->set_n_iterations(ichunk + 1);
  }
  nrows_written_ += nrows_in_chunk;
  xassert(nrows_written_ <= nrows_allocated_);
}


void PreFrame::archive_column_chunks(size_t expected_nrows) {
  if (nrows_written_ == 0) return;
  xassert(nrows_allocated_ > 0);
  size_t memory_limit = g_->memory_bound;

  if (!tempfile_ && memory_limit != MEMORY_UNLIMITED) {
    size_t current_memory = total_allocsize();
    double new_memory = 1.0 * expected_nrows / nrows_allocated_ * current_memory;
    if (new_memory > 0.95 * memory_limit) {
      init_tempfile();
    }
  }
  for (auto& col : columns_) {
    col.archive_data(nrows_written_, tempfile_);
  }
}


void PreFrame::init_tempfile() {
  tempfile_ = std::shared_ptr<TemporaryFile>(new TemporaryFile());
  if (g_->get_verbose()) {
    auto name = tempfile_->name();
    g_->trace("Created temporary file %s", name.c_str());
  }
}




//------------------------------------------------------------------------------
// Iterators
//------------------------------------------------------------------------------

PreColumn& PreFrame::column(size_t i) & {
  xassert(i < columns_.size());
  return columns_[i];
}

const PreColumn& PreFrame::column(size_t i) const & {
  xassert(i < columns_.size());
  return columns_[i];
}


PreFrame::iterator PreFrame::begin() {
  return columns_.begin();
}

PreFrame::iterator PreFrame::end() {
  return columns_.end();
}

PreFrame::const_iterator PreFrame::begin() const {
  return columns_.begin();
}

PreFrame::const_iterator PreFrame::end() const {
  return columns_.end();
}




//------------------------------------------------------------------------------
// Column parse types
//------------------------------------------------------------------------------

std::vector<PT> PreFrame::get_ptypes() const {
  std::vector<PT> res(columns_.size(), PT::Mu);
  save_ptypes(res);
  return res;
}

void PreFrame::save_ptypes(std::vector<PT>& types) const {
  xassert(types.size() == columns_.size());
  size_t i = 0;
  for (auto& col : columns_) {
    types[i++] = col.get_ptype();
  }
}

bool PreFrame::are_same_ptypes(std::vector<PT>& types) const {
  xassert(types.size() == columns_.size());
  size_t i = 0;
  for (auto& col : columns_) {
    if (types[i++] != col.get_ptype()) return false;
  }
  return true;
}

void PreFrame::set_ptypes(const std::vector<PT>& types) {
  xassert(types.size() == columns_.size());
  size_t i = 0;
  for (auto& col : columns_) {
    col.force_ptype(types[i++]);
  }
}

void PreFrame::reset_ptypes() {
  for (auto& col : columns_) {
    col.force_ptype(PT::Mu);
  }
}

const char* PreFrame::print_ptypes() const {
  const ParserInfo* parsers = ParserLibrary::get_parser_infos();
  static const size_t N = 100;
  static char out[N + 1];
  char* ch = out;
  size_t ncols = columns_.size();
  size_t tcols = ncols <= N? ncols : N - 20;
  for (size_t i = 0; i < tcols; ++i) {
    *ch++ = parsers[columns_[i].get_ptype()].code;
  }
  if (tcols != ncols) {
    *ch++ = ' ';
    *ch++ = '.';
    *ch++ = '.';
    *ch++ = '.';
    *ch++ = ' ';
    for (size_t i = ncols - 15; i < ncols; ++i)
      *ch++ = parsers[columns_[i].get_ptype()].code;
  }
  *ch = '\0';
  return out;
}




//------------------------------------------------------------------------------
// Aggregate column stats
//------------------------------------------------------------------------------

size_t PreFrame::n_columns_in_output() const {
  size_t n = 0;
  for (const auto& col : columns_) {
    n += col.is_in_output();
  }
  return n;
}

size_t PreFrame::n_columns_in_buffer() const {
  size_t n = 0;
  for (const auto& col : columns_) {
    n += col.is_in_buffer();
  }
  return n;
}

size_t PreFrame::n_columns_to_reread() const {
  size_t n = 0;
  for (const auto& col : columns_) {
    n += col.is_type_bumped();
  }
  return n;
}


size_t PreFrame::total_allocsize() const {
  size_t allocsize = sizeof(*this);
  for (const auto& col : columns_) {
    allocsize += col.memory_footprint();
  }
  return allocsize;
}




//------------------------------------------------------------------------------
// Finalizing
//------------------------------------------------------------------------------

void PreFrame::prepare_for_rereading() {
  for (auto& col : columns_) {
    col.prepare_for_rereading();
  }
  nrows_written_ = 0;
  nrows_allocated_ = 0;
}


dtptr PreFrame::to_datatable() && {
  std::vector<::Column> ccols;
  std::vector<std::string> names;
  size_t n_outcols = n_columns_in_output();
  ccols.reserve(n_outcols);
  names.reserve(n_outcols);

  std::shared_ptr<TemporaryFile> tmp = nullptr;
  for (auto& col : columns_) {
    if (!col.is_in_output()) continue;
    col.archive_data(nrows_written_, tmp);
    names.push_back(col.get_name());
    ccols.push_back(col.to_column());
  }
  return dtptr(new DataTable(std::move(ccols), std::move(names)));
}




}}  // namespace dt::read
