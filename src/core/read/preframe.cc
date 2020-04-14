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
#include "csv/reader_parsers.h"
#include "read/preframe.h"
#include "column.h"
#include "datatable.h"
namespace dt {
namespace read {



PreFrame::PreFrame() noexcept
  : nrows_(0),
    memory_limit_(size_t(-1)),
    memory_remaining_(size_t(-1)) {}



//------------------------------------------------------------------------------
// Basic properties
//------------------------------------------------------------------------------

size_t PreFrame::ncols() const noexcept {
  return columns_.size();
}

size_t PreFrame::nrows() const noexcept {
  return nrows_;
}


void PreFrame::set_ncols(size_t ncols) {
  xassert(ncols >= columns_.size());
  columns_.resize(ncols);
}

void PreFrame::set_nrows(size_t n) {
  for (auto& col : columns_) {
    col.allocate(n);
  }
  nrows_ = n;
}


void PreFrame::set_memory_bound(size_t sz) {
  memory_limit_ = sz;
  memory_remaining_ = sz;
}

void PreFrame::use_memory_quota(size_t sz) {
  if (memory_remaining_ < sz) {
    memory_remaining_ = memory_limit_;
  }
  memory_remaining_ -= sz;
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

dtptr PreFrame::to_datatable() && {
  std::vector<::Column> ccols;
  std::vector<std::string> names;
  size_t n_outcols = n_columns_in_output();
  ccols.reserve(n_outcols);
  names.reserve(n_outcols);

  for (auto& col : columns_) {
    if (!col.is_in_output()) continue;
    names.push_back(col.get_name());
    ccols.push_back(std::move(col).to_column(nrows_));
  }
  return dtptr(new DataTable(std::move(ccols), std::move(names)));
}




}}  // namespace dt::read
