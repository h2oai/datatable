//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018-2019
//------------------------------------------------------------------------------
#include "read/columns.h"
#include "csv/reader_parsers.h"

namespace dt {
namespace read {


Columns::Columns() noexcept : nrows(0) {}


size_t Columns::size() const noexcept {
  return cols.size();
}

size_t Columns::get_nrows() const noexcept {
  return nrows;
}

void Columns::set_nrows(size_t n) {
  for (auto& col : cols) {
    col.allocate(n);
  }
  nrows = n;
}


Column& Columns::operator[](size_t i) & {
  return cols[i];
}

const Column& Columns::operator[](size_t i) const & {
  return cols[i];
}

void Columns::add_columns(size_t n) {
  cols.reserve(cols.size() + n);
  for (size_t i = 0; i < n; ++i) {
    cols.push_back(Column());
  }
}

std::vector<std::string> Columns::get_names() const {
  std::vector<std::string> names;
  names.reserve(cols.size());
  for (const Column& col : cols) {
    names.push_back(col.get_name());
  }
  return names;
}


//----- Columns types ----------------------------------------------------------

Columns::PTlist Columns::getTypes() const {
  PTlist res(new PT[cols.size()]);
  saveTypes(res);
  return res;
}

void Columns::saveTypes(PTlist& types) const {
  size_t i = 0;
  for (const auto& col : cols) {
    types[i++] = col.get_ptype();
  }
}

bool Columns::sameTypes(PTlist& types) const {
  size_t i = 0;
  for (const auto& col : cols) {
    if (types[i++] != col.get_ptype()) return false;
  }
  return true;
}

void Columns::setTypes(const PTlist& types) {
  size_t i = 0;
  for (auto& col : cols) {
    col.force_ptype(types[i++]);
  }
}

void Columns::setType(PT type) {
  for (auto& col : cols) {
    col.force_ptype(type);
  }
}

const char* Columns::printTypes() const {
  const ParserInfo* parsers = ParserLibrary::get_parser_infos();
  static const size_t N = 100;
  static char out[N + 1];
  char* ch = out;
  size_t ncols = size();
  size_t tcols = ncols <= N? ncols : N - 20;
  for (size_t i = 0; i < tcols; ++i) {
    *ch++ = parsers[cols[i].get_ptype()].code;
  }
  if (tcols != ncols) {
    *ch++ = ' ';
    *ch++ = '.';
    *ch++ = '.';
    *ch++ = '.';
    *ch++ = ' ';
    for (size_t i = ncols - 15; i < ncols; ++i)
      *ch++ = parsers[cols[i].get_ptype()].code;
  }
  *ch = '\0';
  return out;
}


//---- Columns stats -----------------------------------------------------------

size_t Columns::nColumnsInOutput() const {
  size_t n = 0;
  for (const auto& col : cols) {
    n += col.is_in_output();
  }
  return n;
}

size_t Columns::nColumnsInBuffer() const {
  size_t n = 0;
  for (const auto& col : cols) {
    n += col.is_in_buffer();
  }
  return n;
}

size_t Columns::nColumnsToReread() const {
  size_t n = 0;
  for (const auto& col : cols) {
    n += col.is_type_bumped();
  }
  return n;
}

size_t Columns::nStringColumns() const {
  size_t n = 0;
  for (const auto& col : cols) {
    n += col.is_string();
  }
  return n;
}

size_t Columns::totalAllocSize() const {
  size_t allocsize = sizeof(*this);
  for (const auto& col : cols) {
    allocsize += col.memory_footprint();
  }
  return allocsize;
}



}  // namespace read
}  // namespace dt
