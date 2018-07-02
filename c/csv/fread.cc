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
  if (!cc.true_start) {
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
  if (!cc.true_end) {
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


  //*********************************************************************************************
  // [2] Auto detect separator, quoting rule, first line and ncols, simply,
  //     using jump 0 only.
  //
  //     Always sample as if nrows= wasn't supplied. That's probably *why*
  //     user is setting nrow=0 to get the column names and types, without
  //     actually reading the data yet. Most likely to check consistency
  //     across a set of files.
  //*********************************************************************************************
  {
    if (verbose) trace("[2] Detect separator, quoting rule, and ncolumns");

    int nseps;
    char seps[] = ",|;\t ";  // default seps in order of preference. See ?fread.
    char topSep;             // which sep matches the input best so far
    // using seps[] not *seps for writeability (http://stackoverflow.com/a/164258/403310)

    if (sep == '\xFF') {   // '\xFF' means 'auto'
      nseps = (int) strlen(seps);
      topSep = '\xFE';     // '\xFE' means single-column mode
    } else {
      // Cannot use '\n' as a separator, because it prevents us from proper
      // detection of line endings
      if (sep == '\n') sep = '\xFE';
      seps[0] = sep;
      seps[1] = '\0';
      topSep = sep;
      nseps = 1;
      trace("Using supplied sep '%s'",
            sep=='\t' ? "\\t" : sep=='\xFE' ? "\\n" : seps);
    }

    const char* firstJumpEnd = nullptr; // remember where the winning jumpline from jump 0 ends, to know its size excluding header
    int topNumLines = 0;      // the most number of lines with the same number of fields, so far
    int topNumFields = 0;     // how many fields that was, to resolve ties
    int8_t topQuoteRule = -1;  // which quote rule that was
    int topNmax=1;            // for that sep and quote rule, what was the max number of columns (just for fill=true)
                              //   (when fill=true, the max is usually the header row and is the longest but there are more
                              //    lines of fewer)

    field64 trash;
    FreadTokenizer ctx = makeTokenizer(&trash, nullptr);
    const char*& tch = ctx.ch;

    // We will scan the input line-by-line (at most `JUMPLINES + 1` lines; "+1"
    // covers the header row, at this stage we don't know if it's present), and
    // detect the number of fields on each line. If several consecutive lines
    // have the same number of fields, we'll call them a "contiguous group of
    // lines". Arrays `numFields` and `numLines` contain information about each
    // contiguous group of lines encountered while scanning the first JUMPLINES
    // + 1 lines: 'numFields` gives the count of fields in each group, and
    // `numLines` has the number of lines in each group.
    int numFields[JUMPLINES+1];
    int numLines[JUMPLINES+1];
    for (quoteRule=0; quoteRule<4; quoteRule++) {  // quote rule in order of preference
      for (int s=0; s<nseps; s++) {
        sep = seps[s];
        whiteChar = (sep==' ' ? '\t' : (sep=='\t' ? ' ' : 0));  // 0 means both ' ' and '\t' to be skipped
        ctx.ch = sof;
        ctx.sep = sep;
        ctx.whiteChar = whiteChar;
        ctx.quoteRule = quoteRule;
        // if (verbose) trace("  Trying sep='%c' with quoteRule %d ...\n", sep, quoteRule);
        for (int i=0; i<=JUMPLINES; i++) { numFields[i]=0; numLines[i]=0; } // clear VLAs
        int i=-1; // The slot we're counting the currently contiguous consistent ncols
        int thisLine=0, lastncol=-1;
        while (tch < eof && thisLine++ < JUMPLINES) {
          // Compute num columns and move `tch` to the start of next line
          int thisncol = ctx.countfields();
          if (thisncol < 0) {
            // invalid file with this sep and quote rule; abort
            numFields[0] = -1;
            break;
          }
          if (thisncol != lastncol) {  // new contiguous consistent ncols started
            numFields[++i] = thisncol;
            lastncol = thisncol;
          }
          numLines[i]++;
        }
        if (numFields[0] == -1) continue;
        if (firstJumpEnd == nullptr) firstJumpEnd = tch;  // if this wins (doesn't get updated), it'll be single column input
        if (topQuoteRule < 0) topQuoteRule = quoteRule;
        bool updated = false;
        int nmax = 0;

        i = -1;
        while (numLines[++i]) {
          if (numFields[i] > nmax) {  // for fill=true to know max number of columns
            nmax = numFields[i];
          }
          if ( numFields[i]>1 &&
              (numLines[i]>1 || (/*blank line after single line*/numFields[i+1]==0)) &&
              ((numLines[i]>topNumLines) ||   // most number of consistent ncols wins
               (numLines[i]==topNumLines && numFields[i]>topNumFields && sep!=topSep && sep!=' '))) {
               //                                       ^ ties in numLines resolved by numFields (more fields win)
               //                                                           ^ but don't resolve a tie with a higher quote
               //                                                             rule unless the sep is different too: #2404, #2839
            topNumLines = numLines[i];
            topNumFields = numFields[i];
            topSep = sep;
            topQuoteRule = quoteRule;
            topNmax = nmax;
            firstJumpEnd = tch;  // So that after the header we know how many bytes jump point 0 is
            updated = true;
            // Two updates can happen for the same sep and quoteRule (e.g. issue_1113_fread.txt where sep=' ') so the
            // updated flag is just to print once.
          } else if (topNumFields == 0 && nseps == 1 && quoteRule != 2) {
            topNumFields = numFields[i];
            topSep = sep;
            topQuoteRule = quoteRule;
            topNmax = nmax;
          }
        }
        if (verbose && updated) {
          trace(sep<' '? "sep='\\x%02x' with %d lines of %d fields using quote rule %d" :
                         "sep='%c' with %d lines of %d fields using quote rule %d",
                sep, topNumLines, topNumFields, topQuoteRule);
        }
      }
    }
    if (!topNumFields) topNumFields = 1;
    xassert(firstJumpEnd && topQuoteRule >= 0);
    quoteRule = ctx.quoteRule = topQuoteRule;
    sep = ctx.sep = topSep;
    whiteChar = ctx.whiteChar = (sep==' ' ? '\t' : (sep=='\t' ? ' ' : 0));
    if (sep==' ' && !fill) {
      trace("sep=' ' detected, setting fill to True");
      fill = 1;
    }

    int ncols = fill? topNmax : topNumFields;
    xassert(ncols >= 1 && line >= 1);

    // Create vector of Column objects
    columns.add_columns(static_cast<size_t>(ncols));

    first_jump_size = static_cast<size_t>(firstJumpEnd - sof);

    if (verbose) {
      trace("Detected %d columns", ncols);
      if (sep == '\xFE') trace("sep = <single-column mode>");
      else if (sep >= ' ') trace("sep = '%c'", sep);
      else trace("sep = '\\x%02x'", int(sep));
      trace("Quote rule = %d", quoteRule);
      fo.t_parse_parameters_detected = wallclock();
    }

    // if (ncols > 1) {
    //   // Don't do this for ncols = 1, because then blank lines are significant.
    //   skip_trailing_whitespace();
    // }
  }


  detect_column_types();  // [3]


  //*********************************************************************************************
  // [4] Parse column names (if present)
  //
  //     This section also moves the `sof` pointer to point at the first row
  //     of data ("removing" the column names).
  //*********************************************************************************************
  if (header == 1) {
    trace("[4] Assign column names");
    field64 tmp;
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
      GReaderColumn& col = columns[i];
      if (col.is_dropped()) {
        ndropped++;
        continue;
      } else {
        if (col.ptype < oldtypes[i]) {
          // FIXME: if the user wants to override the type, let them
          STOP("Attempt to override column %d \"%s\" of inherent type '%s' down to '%s' which will lose accuracy. " \
               "If this was intended, please coerce to the lower type afterwards. Only overrides to a higher type are permitted.",
               i+1, col.repr_name(*this), ParserLibrary::info(oldtypes[i]).cname(), col.typeName());
        }
        nUserBumped += (col.ptype != oldtypes[i]);
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
  int typeCounts[ParserLibrary::num_parsers];  // used for verbose output

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

      for (size_t i = 0; i < ParserLibrary::num_parsers; ++i) typeCounts[i] = 0;
      for (size_t i = 0; i < ncols; i++) {
        typeCounts[columns[i].ptype]++;
      }

      size_t ncols_to_reread = columns.nColumnsToReread();
      if (ncols_to_reread) {
        fo.n_cols_reread += ncols_to_reread;
        size_t n_type_bump_cols = 0;
        for (size_t j = 0; j < ncols; j++) {
          GReaderColumn& col = columns[j];
          if (!col.presentInOutput) continue;
          if (col.typeBumped) {
            // column was bumped due to out-of-sample type exception
            col.typeBumped = false;
            col.presentInBuffer = true;
            n_type_bump_cols++;
          } else {
            col.presentInBuffer = false;
          }
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
