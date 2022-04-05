//------------------------------------------------------------------------------
// Copyright 2018-2021 H2O.ai
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
#include "column.h"
#include "csv/reader.h"              // GenericReader
#include "datatable.h"
#include "read/preframe.h"
#include "read/parsers/info.h"
#include "utils/temporary_file.h"    // TemporaryFile
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
  size_t memory_limit = g_->memory_limit;

  // Possibly reduce memory allocation if there is a memory limit
  if (memory_limit != MEMORY_UNLIMITED) {
    size_t row_size = 0;
    for (const auto& col : columns_) {
      row_size += col.elemsize() * (1 + col.is_string());
    }
    if (row_size * nrows > memory_limit) {
      nrows = std::max(size_t(1), memory_limit / row_size);
      if (g_->verbose) {
        g_->d() << "Allocation size reduced to " << nrows
                << " rows due to memory_limit parameter";
      }
    }
  }
  for (auto& col : columns_) {
    col.outcol().allocate(nrows);
  }
  nrows_allocated_ = nrows;
}


/**
  * Make sure there is enough room in the columns to write
  * `nrows_in_chunk0` rows. The actual number of rows written will be
  * returned. This number may be less than `nrows_in_chunk0` if the
  * total number of rows exceeds `max_nrows` parameter.
  *
  * The `otask` variable allows us to retrieve information about the
  * current state of iteration, and to wait until the currently
  * pending data is safely written if we need to reallocate buffers.
  *
  * This function will also adjust the `nrows_written` counter, and
  * thus should be called from the ordered section only.
  */
size_t PreFrame::ensure_output_nrows(size_t nrows_in_chunk0, size_t ichunk,
                                     dt::OrderedTask* otask)
{
  size_t nrows_in_chunk = nrows_in_chunk0;  // may be changed due to max_nrows
  size_t nrows_new = nrows_written_ + nrows_in_chunk;
  size_t nrows_max = g_->max_nrows;
  size_t memory_limit = g_->memory_limit;

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
    size_t nchunks = otask->get_num_iterations();
    xassert(ichunk < nchunks);
    if (ichunk < nchunks - 1) {
      nrows_new = std::min(
          nrows_max,
          std::max(
              1024 + nrows_allocated_,
              static_cast<size_t>(1.2 * static_cast<double>(nrows_new)
                                      * static_cast<double>(nchunks)
                                      / static_cast<double>(ichunk + 1))
      ));
    }

    xassert(nrows_new >= nrows_in_chunk + nrows_written_);
    otask->wait_until_all_finalized();
    archive_column_chunks(nrows_new);

    // If there is a memory limit, then we potentially need to adjust
    // the number of rows allocated, so as not to exceed this limit.
    if (memory_limit != MEMORY_UNLIMITED) {
      size_t nrows_extra = nrows_new - nrows_written_;
      size_t archived_size = 0;
      for (const auto& col : columns_) {
        archived_size += col.archived_size();
      }
      double avg_size_per_row = static_cast<double>(archived_size)
                                / static_cast<double>(nrows_written_);
      if (static_cast<double>(nrows_extra) * avg_size_per_row > static_cast<double>(memory_limit)) {
        nrows_extra = std::max(
            nrows_in_chunk,
            static_cast<size_t>(static_cast<double>(memory_limit)
                                / avg_size_per_row)
        );
        nrows_new = nrows_written_ + nrows_extra;
      }
    }

    if (g_->verbose) {
      g_->d() << "Too few rows allocated, reallocating to "
              << nrows_new << " rows";
    }

    // Now reallocate all columns for a proper number of rows
    for (auto& col : columns_) {
      col.outcol().allocate(nrows_new);
    }
    nrows_allocated_ = nrows_new;
  }

  if (nrows_new == nrows_max) {
    otask->set_num_iterations(ichunk + 1);
  }
  nrows_written_ += nrows_in_chunk;
  xassert(nrows_written_ <= nrows_allocated_);
  return nrows_in_chunk;
}


void PreFrame::archive_column_chunks(size_t expected_nrows) {
  if (nrows_written_ == 0) return;
  xassert(nrows_allocated_ > 0);
  size_t memory_limit = g_->memory_limit;

  if (!tempfile_ && memory_limit != MEMORY_UNLIMITED) {
    size_t current_memory = total_allocsize();
    double new_memory = static_cast<double>(expected_nrows)
        / static_cast<double>(nrows_allocated_)
        * static_cast<double>(current_memory);
    if (new_memory > 0.95 * static_cast<double>(memory_limit)) {
      init_tempfile();
    }
  }
  for (auto& col : columns_) {
    col.outcol().archive_data(nrows_written_, tempfile_);
  }
}


void PreFrame::init_tempfile() {
  auto tempdir = g_->get_tempdir();
  tempfile_ = std::shared_ptr<TemporaryFile>(new TemporaryFile(tempdir));
  if (g_->get_verbose()) {
    auto name = tempfile_->name();
    g_->d() << "Created temporary file " << name;
  }
}


std::shared_ptr<TemporaryFile>& PreFrame::get_tempfile() {
  return tempfile_;
}





//------------------------------------------------------------------------------
// Iterators
//------------------------------------------------------------------------------

InputColumn& PreFrame::column(size_t i) & {
  xassert(i < columns_.size());
  return columns_[i];
}

const InputColumn& PreFrame::column(size_t i) const & {
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
  std::vector<PT> res(columns_.size(), PT::Void);
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
    col.set_ptype(types[i++]);
    col.outcol().set_stype(col.get_stype());
  }
}

void PreFrame::reset_ptypes() {
  for (auto& col : columns_) {
    col.set_ptype(PT::Void);
    col.outcol().set_stype(col.get_stype());
  }
}

const char* PreFrame::print_ptypes() const {
  static const size_t N = 100;
  static char out[N + 1];
  char* ch = out;
  size_t ncols = columns_.size();
  size_t tcols = ncols <= N? ncols : N - 20;
  for (size_t i = 0; i < tcols; ++i) {
    *ch++ = parser_infos[columns_[i].get_ptype()].code();
  }
  if (tcols != ncols) {
    *ch++ = ' ';
    *ch++ = '.';
    *ch++ = '.';
    *ch++ = '.';
    *ch++ = ' ';
    for (size_t i = ncols - 15; i < ncols; ++i)
      *ch++ = parser_infos[columns_[i].get_ptype()].code();
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
    n += !col.is_dropped();
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

dtptr PreFrame::to_datatable() && {
  std::vector<::Column> ccols;
  std::vector<std::string> names;
  size_t n_outcols = n_columns_in_output();
  ccols.reserve(n_outcols);
  names.reserve(n_outcols);

  if (tempfile_) {
    tempfile_->data_r();  // Make sure tempfile is initialized for reading
    tempfile_ = nullptr;
  }
  for (auto& col : columns_) {
    if (col.is_dropped()) continue;
    auto& outcol = col.outcol();
    col.outcol().archive_data(nrows_written_, tempfile_);
    names.push_back(col.get_name());
    ccols.push_back(outcol.to_column());
  }
  return dtptr(new DataTable(std::move(ccols), std::move(names)));
}




}}  // namespace dt::read
