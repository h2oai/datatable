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

