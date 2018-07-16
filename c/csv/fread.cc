//------------------------------------------------------------------------------
// Copyright 2017 data.table authors
// (https://github.com/Rdatatable/data.table/DESCRIPTION)
//
// This Source Code Form is subject to the terms of the Mozilla Public License,
// v.2.0. If a copy of the MPL was not distributed with this file, You can
// obtain one at https://mozilla.org/MPL/2.0/.
//------------------------------------------------------------------------------
#include "csv/fread.h"
#include "csv/freadLookups.h"
#include "csv/reader.h"
#include "csv/reader_fread.h"
#include "csv/reader_parsers.h"
#include <ctype.h>     // isspace
#include <stdarg.h>    // va_list, va_start
#include <stdio.h>     // vsnprintf
#include <algorithm>
#include <cmath>       // std::sqrt, std::ceil
#include <cstdio>      // std::snprintf
#include <string>      // std::string
#include "utils/assert.h"



//------------------------------------------------------------------------------


// Find the next "good line", in the sense that we find at least 5 lines
// with `ncols` fields from that point on.
bool FreadChunkedReader::next_good_line_start(
  const ChunkCoordinates& cc, FreadTokenizer& tokenizer) const
{
  int ncols = static_cast<int>(f.get_ncols());
  bool fill = f.fill;
  bool skipEmptyLines = f.skip_blank_lines;
  const char*& ch = tokenizer.ch;
  ch = cc.start;
  const char* eof = cc.end;
  int attempts = 0;
  while (ch < eof && attempts++ < 10) {
    while (ch < eof && *ch != '\n' && *ch != '\r') ch++;
    if (ch == eof) break;
    tokenizer.skip_eol();  // updates `ch`
    // countfields() below moves the parse location, so store it in `ch1` in
    // order to revert to the current parsing location later.
    const char* ch1 = ch;
    int i = 0;
    for (; i < 5; ++i) {
      // `countfields()` advances `ch` to the beginning of the next line
      int n = tokenizer.countfields();
      if (n != ncols &&
          !(ncols == 1 && n == 0) &&
          !(skipEmptyLines && n == 0) &&
          !(fill && n < ncols)) break;
    }
    ch = ch1;
    // `i` is the count of consecutive consistent rows
    if (i == 5) return true;
  }
  return false;
}



void FreadChunkedReader::adjust_chunk_coordinates(
  ChunkCoordinates& cc, LocalParseContext* ctx) const
{
  // Adjust the beginning of the chunk so that it is guaranteed not to be
  // on a newline.
  if (!cc.start_exact) {
    auto fctx = static_cast<FreadLocalParseContext*>(ctx);
    const char* start = cc.start;
    while (*start=='\n' || *start=='\r') start++;
    cc.start = start;
    if (next_good_line_start(cc, fctx->tokenizer)) {
      cc.start = fctx->tokenizer.ch;
    }
  }
  // Move the end of the chunk, similarly skipping all newline characters;
  // plus 1 more character, thus guaranteeing that the entire next line will
  // also "belong" to the current chunk (this because chunk reader stops at
  // the first end of the line after `end`).
  if (!cc.end_exact) {
    const char* end = cc.end;
    while (*end=='\n' || *end=='\r') end++;
    cc.end = end + 1;
  }
}




//=================================================================================================
//
// Main fread() function that does all the job of reading a text/csv file.
//
// Returns 1 if it finishes successfully, and 0 otherwise.
//
//=================================================================================================
DataTablePtr FreadReader::read()
{
  detect_lf();
  skip_preamble();

  if (verbose) fo.t_initialized = wallclock();

  detect_sep_and_qr();    // [2]
  detect_column_types();  // [3]

  //*********************************************************************************************
  // [4] Parse column names (if present)
  //
  //     This section also moves the `sof` pointer to point at the first row
  //     of data ("removing" the column names).
  //*********************************************************************************************
  if (header == 1) {
    trace("[4] Assign column names");
    dt::read::field64 tmp;
    FreadTokenizer fctx = makeTokenizer(&tmp, /* anchor= */ sof);
    fctx.ch = sof;
    parse_column_names(fctx);
    sof = fctx.ch;  // Update sof to point to the first line after the columns
    line++;
  }
  if (verbose) fo.t_column_types_detected = wallclock();


  //*********************************************************************************************
  // [5] Allow user to override column types; then allocate the DataTable
  //*********************************************************************************************
  {
    if (verbose) trace("[5] Apply user overrides on column types");
    std::unique_ptr<PT[]> oldtypes = columns.getTypes();

    report_columns_to_python();

    size_t ncols = columns.size();
    size_t ndropped = 0;
    int nUserBumped = 0;
    for (size_t i = 0; i < ncols; i++) {
      dt::read::Column& col = columns[i];
      col.reset_type_bumped();
      if (col.is_dropped()) {
        ndropped++;
        continue;
      } else {
        if (col.get_ptype() < oldtypes[i]) {
          // FIXME: if the user wants to override the type, let them
          STOP("Attempt to override column %d \"%s\" of inherent type '%s' down to '%s' which will lose accuracy. " \
               "If this was intended, please coerce to the lower type afterwards. Only overrides to a higher type are permitted.",
               i+1, col.repr_name(*this), ParserLibrary::info(oldtypes[i]).cname(), col.typeName());
        }
        nUserBumped += (col.get_ptype() != oldtypes[i]);
      }
    }
    if (verbose) {
      trace("After %d type and %d drop user overrides : %s",
            nUserBumped, ndropped, columns.printTypes());
      trace("Allocating %d column slots with %zd rows",
            ncols - ndropped, allocnrow);
    }

    columns.set_nrows(allocnrow);

    if (verbose) {
      fo.t_frame_allocated = wallclock();
      fo.n_rows_allocated = allocnrow;
      fo.n_cols_allocated = ncols - ndropped;
      fo.allocation_size = columns.totalAllocSize();
    }
  }


  //*********************************************************************************************
  // [6] Read the data
  //*********************************************************************************************
  bool firstTime = true;

  std::unique_ptr<PT[]> typesPtr = columns.getTypes();
  PT* types = typesPtr.get();  // This pointer is valid until `typesPtr` goes out of scope

  trace("[6] Read the data");
  read:  // we'll return here to reread any columns with out-of-sample type exceptions
  {
    FreadChunkedReader scr(*this, types);
    scr.read_all();

    if (firstTime) {
      fo.t_data_read = fo.t_data_reread = wallclock();
      size_t ncols = columns.size();
      size_t ncols_to_reread = columns.nColumnsToReread();
      if (ncols_to_reread) {
        fo.n_cols_reread += ncols_to_reread;
        size_t n_type_bump_cols = 0;
        for (size_t j = 0; j < ncols; j++) {
          dt::read::Column& col = columns[j];
          if (!col.is_in_output()) continue;
          bool bumped = col.is_type_bumped();
          col.reset_type_bumped();
          col.set_in_buffer(bumped);
          n_type_bump_cols += bumped;
        }
        firstTime = false;
        if (verbose) {
          trace(n_type_bump_cols == 1
                ? "%zu column needs to be re-read because its type has changed"
                : "%zu columns need to be re-read because their types have changed",
                n_type_bump_cols);
        }
        goto read;
      }
    } else {
      fo.t_data_reread = wallclock();
    }

    fo.n_rows_read = columns.get_nrows();
    fo.n_cols_read = columns.nColumnsInOutput();
  }


  trace("[7] Finalize the datatable");
  DataTablePtr res = makeDatatable();
  if (verbose) fo.report();
  return res;
}
