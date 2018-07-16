//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "csv/reader.h"
#include "csv/fread.h"   // temporary
#include "csv/reader_parsers.h"
#include "python/string.h"
#include "python/long.h"
#include "utils/assert.h"
#include "utils/exceptions.h"
#include "utils/omp.h"



//------------------------------------------------------------------------------
// GReaderColumns
//------------------------------------------------------------------------------

GReaderColumns::GReaderColumns() noexcept : allocnrows(0) {}


size_t GReaderColumns::size() const noexcept {
  return cols.size();
}

size_t GReaderColumns::get_nrows() const noexcept {
  return allocnrows;
}

void GReaderColumns::set_nrows(size_t nrows) {
  for (auto& col : cols) {
    col.allocate(nrows);
  }
  allocnrows = nrows;
}


dt::read::Column& GReaderColumns::operator[](size_t i) & {
  return cols[i];
}

const dt::read::Column& GReaderColumns::operator[](size_t i) const & {
  return cols[i];
}

void GReaderColumns::add_columns(size_t n) {
  cols.reserve(cols.size() + n);
  for (size_t i = 0; i < n; ++i) {
    cols.push_back(dt::read::Column());
  }
}


std::unique_ptr<PT[]> GReaderColumns::getTypes() const {
  std::unique_ptr<PT[]> res(new PT[cols.size()]);
  saveTypes(res);
  return res;
}

void GReaderColumns::saveTypes(std::unique_ptr<PT[]>& types) const {
  size_t i = 0;
  for (const auto& col : cols) {
    types[i++] = col.get_ptype();
  }
}

bool GReaderColumns::sameTypes(std::unique_ptr<PT[]>& types) const {
  size_t i = 0;
  for (const auto& col : cols) {
    if (types[i++] != col.get_ptype()) return false;
  }
  return true;
}

void GReaderColumns::setTypes(const std::unique_ptr<PT[]>& types) {
  size_t i = 0;
  for (auto& col : cols) {
    col.force_ptype(types[i++]);
  }
}

void GReaderColumns::setType(PT type) {
  for (auto& col : cols) {
    col.force_ptype(type);
  }
}

const char* GReaderColumns::printTypes() const {
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

size_t GReaderColumns::nColumnsInOutput() const {
  size_t n = 0;
  for (const auto& col : cols) {
    n += col.is_in_output();
  }
  return n;
}

size_t GReaderColumns::nColumnsInBuffer() const {
  size_t n = 0;
  for (const auto& col : cols) {
    n += col.is_in_buffer();
  }
  return n;
}

size_t GReaderColumns::nColumnsToReread() const {
  size_t n = 0;
  for (const auto& col : cols) {
    n += col.is_type_bumped();
  }
  return n;
}

size_t GReaderColumns::nStringColumns() const {
  size_t n = 0;
  for (const auto& col : cols) {
    n += col.is_string();
  }
  return n;
}

size_t GReaderColumns::totalAllocSize() const {
  size_t allocsize = sizeof(*this);
  for (const auto& col : cols) {
    allocsize += col.memory_footprint();
  }
  return allocsize;
}




//------------------------------------------------------------------------------
// LocalParseContext
//------------------------------------------------------------------------------

LocalParseContext::LocalParseContext(size_t ncols, size_t nrows)
  : tbuf(ncols * nrows + 1), sbuf(0), strinfo(ncols)
{
  tbuf_ncols = ncols;
  tbuf_nrows = nrows;
  used_nrows = 0;
  row0 = 0;
}


LocalParseContext::~LocalParseContext() {
  if (used_nrows != 0) {
    printf("Assertion error in ~LocalParseContext(): used_nrows != 0\n");
  }
}


void LocalParseContext::allocate_tbuf(size_t ncols, size_t nrows) {
  tbuf.resize(ncols * nrows + 1);
  tbuf_ncols = ncols;
  tbuf_nrows = nrows;
}


size_t LocalParseContext::get_nrows() const {
  return used_nrows;
}


void LocalParseContext::set_nrows(size_t n) {
  xassert(n <= used_nrows);
  used_nrows = n;
}

