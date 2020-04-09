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



PreFrame::PreFrame() noexcept : nrows_(0) {}


size_t PreFrame::size() const noexcept {
  return cols_.size();
}

size_t PreFrame::get_nrows() const noexcept {
  return nrows_;
}

void PreFrame::set_nrows(size_t n) {
  for (auto& col : cols_) {
    col.allocate(n);
  }
  nrows_ = n;
}


Column& PreFrame::operator[](size_t i) & {
  xassert(i < cols_.size());
  return cols_[i];
}

const Column& PreFrame::operator[](size_t i) const & {
  return cols_[i];
}

void PreFrame::add_columns(size_t n) {
  cols_.reserve(cols_.size() + n);
  for (size_t i = 0; i < n; ++i) {
    cols_.push_back(Column());
  }
}



//----- Columns types ----------------------------------------------------------

std::vector<PT> PreFrame::getTypes() const {
  std::vector<PT> res(cols_.size(), PT::Mu);
  saveTypes(res);
  return res;
}

void PreFrame::saveTypes(std::vector<PT>& types) const {
  xassert(types.size() == cols_.size());
  for (size_t i = 0; i < cols_.size(); ++i) {
    types[i] = cols_[i].get_ptype();
  }
}

bool PreFrame::sameTypes(std::vector<PT>& types) const {
  xassert(types.size() == cols_.size());
  for (size_t i = 0; i < cols_.size(); ++i) {
    if (types[i] != cols_[i].get_ptype()) return false;
  }
  return true;
}

void PreFrame::setTypes(const std::vector<PT>& types) {
  xassert(types.size() == cols_.size());
  for (size_t i = 0; i < cols_.size(); ++i) {
    cols_[i].force_ptype(types[i]);
  }
}

void PreFrame::setType(PT type) {
  for (auto& col : cols_) {
    col.force_ptype(type);
  }
}

const char* PreFrame::printTypes() const {
  const ParserInfo* parsers = ParserLibrary::get_parser_infos();
  static const size_t N = 100;
  static char out[N + 1];
  char* ch = out;
  size_t ncols = size();
  size_t tcols = ncols <= N? ncols : N - 20;
  for (size_t i = 0; i < tcols; ++i) {
    *ch++ = parsers[cols_[i].get_ptype()].code;
  }
  if (tcols != ncols) {
    *ch++ = ' ';
    *ch++ = '.';
    *ch++ = '.';
    *ch++ = '.';
    *ch++ = ' ';
    for (size_t i = ncols - 15; i < ncols; ++i)
      *ch++ = parsers[cols_[i].get_ptype()].code;
  }
  *ch = '\0';
  return out;
}


//---- Columns stats -----------------------------------------------------------

size_t PreFrame::nColumnsInOutput() const {
  size_t n = 0;
  for (const auto& col : cols_) {
    n += col.is_in_output();
  }
  return n;
}

size_t PreFrame::nColumnsInBuffer() const {
  size_t n = 0;
  for (const auto& col : cols_) {
    n += col.is_in_buffer();
  }
  return n;
}

size_t PreFrame::nColumnsToReread() const {
  size_t n = 0;
  for (const auto& col : cols_) {
    n += col.is_type_bumped();
  }
  return n;
}

size_t PreFrame::nStringColumns() const {
  size_t n = 0;
  for (const auto& col : cols_) {
    n += col.is_string();
  }
  return n;
}

size_t PreFrame::totalAllocSize() const {
  size_t allocsize = sizeof(*this);
  for (const auto& col : cols_) {
    allocsize += col.memory_footprint();
  }
  return allocsize;
}



//---- Finalizing --------------------------------------------------------------
using dtptr = std::unique_ptr<DataTable>;

dtptr PreFrame::to_datatable() {
  std::vector<::Column> ccols;
  std::vector<std::string> names;
  size_t n_outcols = nColumnsInOutput();
  ccols.reserve(n_outcols);
  names.reserve(n_outcols);

  for (size_t i = 0; i < cols_.size(); ++i) {
    auto& col = cols_[i];
    if (!col.is_in_output()) continue;
    Buffer databuf = col.extract_databuf();
    Buffer strbuf = col.extract_strbuf();
    SType stype = col.get_stype();
    ccols.push_back((stype == SType::STR32 || stype == SType::STR64)
      ? ::Column::new_string_column(nrows_, std::move(databuf), std::move(strbuf))
      : ::Column::new_mbuf_column(nrows_, stype, std::move(databuf))
    );
    names.push_back(col.get_name());
  }
  return dtptr(new DataTable(std::move(ccols), std::move(names)));
}




}}  // namespace dt::read
