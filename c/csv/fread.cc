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
#include "utils/shared_mutex.h"


#define JUMPLINES 100    // at each of the 100 jumps how many lines to guess column types (10,000 sample lines)

/**
 * Helper for error and warning messages to extract an input line starting at
 * `*ch` and until an end of line, but no longer than `limit` characters.
 * This function returns the string copied into an internal static buffer. Cannot
 * be called more than twice per single printf() invocation.
 * Parameter `limit` cannot exceed 500.
 */
const char* strlim(const char* ch, size_t limit) {
  static char buf[1002];
  static int flip = 0;
  char* ptr = buf + 501 * flip;
  flip = 1 - flip;
  char* ch2 = ptr;
  size_t width = 0;
  while ((*ch>'\r' || (*ch!='\0' && *ch!='\r' && *ch!='\n')) && width++<limit) *ch2++ = *ch++;
  *ch2 = '\0';
  return ptr;
}


typedef std::unique_ptr<LocalParseContext>LPCPtr;
typedef std::unique_ptr<FreadLocalParseContext> FLPCPtr;


//------------------------------------------------------------------------------

class FreadChunkedReader {
  private:
    FreadReader& f;
    ChunkOrganizerPtr chunkster;
  public:
    // dt::shared_mutex shmutex;
    size_t rowSize;
    size_t chunk0;
    int buffGrown;
    int : 32;
    static constexpr size_t stopErrSize = 1000;
    char stopErr[stopErrSize];
    char* typeBumpMsg;
    size_t typeBumpMsgSize;
    int8_t* types;
    int nTypeBump;
    int nTypeBumpCols;
    size_t row0;
    size_t allocnrow;
    size_t max_nrows;
    double thRead, thPush;

  public:
    // The abominable constructor
    FreadChunkedReader(
        FreadReader& reader, size_t rowSize_,
        const char* lastRowEnd_, char* typeBumpMsg_, size_t typeBumpMsgSize_,
        int8_t* types_, size_t allocnrow_
    ) : f(reader)
    {
      chunkster = init_chunk_organizer(f.sof, lastRowEnd_);
      rowSize = rowSize_;
      types = types_;
      allocnrow = allocnrow_;
      max_nrows = f.max_nrows;
      chunk0 = 0;
      buffGrown = 0;
      stopErr[0] = '\0';
      typeBumpMsg = typeBumpMsg_;
      typeBumpMsgSize = typeBumpMsgSize_;
      nTypeBump = 0;
      nTypeBumpCols = 0;
      row0 = 0;
      thRead = 0.0;
      thPush = 0.0;
      ASSERT(allocnrow <= max_nrows);
    }
    ~FreadChunkedReader() {}

    std::unique_ptr<FreadLocalParseContext> init_thread_context() {
      size_t nchunks = chunkster->get_nchunks();
      size_t trows = std::max<size_t>(allocnrow / nchunks, 4);
      size_t tcols = rowSize / 8;
      return FLPCPtr(new FreadLocalParseContext(tcols, trows, f, types,
        typeBumpMsg, typeBumpMsgSize, stopErr, stopErrSize));
    }

    ChunkOrganizerPtr init_chunk_organizer(
        const char* inputStart, const char* inputEnd)
    {
      return ChunkOrganizerPtr(
        new FreadChunkOrganizer(inputStart, inputEnd, f)
      );
    }


    //********************************//
    // Main function
    //********************************//
    void read_all()
    {
      // If we need to restart reading the file because we ran out of allocation
      // space, then this variable will tell how many new rows has to be allocated.
      size_t extraAllocRows = 0;
      bool stopTeam = false;
      size_t nchunks = 0;
      bool progressShown = false;
      OmpExceptionManager oem;
      int nthreads = chunkster->get_nthreads();
      if (nthreads != f.nthreads) {
        f.trace("Number of threads reduced to %d because data is small",
                nthreads);
      }

      #pragma omp parallel num_threads(nthreads)
      {
        bool tMaster = false;
        #pragma omp master
        {
          int actualNthreads = omp_get_num_threads();
          if (actualNthreads != nthreads) {
            nthreads = actualNthreads;
            chunkster->set_nthreads(nthreads);
            f.trace("Actual number of threads allowed by OMP: %d", nthreads);
          }
          nchunks = chunkster->get_nchunks();
          tMaster = true;
        }
        // Wait for master here: we want all threads to have consistent
        // view of the chunking parameters.
        #pragma omp barrier

        bool tShowProgress = f.report_progress && tMaster;
        bool tShowAlways = false;
        double tShowWhen = tShowProgress? wallclock() + 0.75 : 0;

        FLPCPtr ctx = init_thread_context();
        ChunkCoordinates xcc;
        ChunkCoordinates acc;

        #pragma omp for ordered schedule(dynamic) reduction(+:thRead,thPush)
        for (size_t i = chunk0; i < nchunks; i++) {
          if (stopTeam) continue;
          try {
            if (tShowAlways || (tShowProgress && wallclock() >= tShowWhen)) {
              f.progress(chunkster->work_done_amount());
              tShowAlways = true;
            }

            ctx->push_buffers();
            xcc = chunkster->compute_chunk_boundaries(i, ctx.get());
            ctx->read_chunk(xcc, acc);

          } catch (...) {
            oem.capture_exception();
            stopTeam = true;
          }

          #pragma omp ordered
          do {
            try {
              // The `is_ordered()` call checks whether the actual start of the
              // chunk was correct (i.e. there are no gaps/overlaps in the
              // input). If not, then we re-read the chunk using the correct
              // coordinates (which `is_ordered()` saves in variable `xcc`).
              // We also re-read if the first `read_chunk()` call returned an
              // error: even if the chunk's start was determined correctly, we
              // didn't know that up to this point, and so were not producing
              // the correct error message.
              // After re-reading, it is possible to still have `acc.end` equal
              // nullptr (i.e. genuine file reading error); otherwise `acc.end`
              // will have the correct end of the current chunk, which MUST be
              // reported to `chunkster` by calling `is_ordered()` the second
              // time.
              bool reparse_error = !acc.end && !xcc.true_start;
              if (!chunkster->is_ordered(acc, xcc) || reparse_error) {
                xassert(xcc.true_start);
                ctx->read_chunk(xcc, acc);
                bool ok = !acc.end || chunkster->is_ordered(acc, xcc);
                xassert(ok);
              }
              if (!acc.end) {
                xassert(stopErr[0]);
                stopTeam = true;
                break;
              }
              ctx->row0 = row0;  // fetch shared row0 (where to write my results to the answer).
              if (ctx->row0 >= allocnrow) {  // a previous thread has already reached the `allocnrow` limit
                stopTeam = true;
                ctx->used_nrows = 0;
              } else if (ctx->used_nrows + ctx->row0 > allocnrow) {  // current thread has reached `allocnrow` limit
                if (allocnrow == max_nrows) {
                  // allocnrow is the same as max_nrows, no need to reallocate the DT,
                  // just truncate the rows in the current chunk.
                  ctx->used_nrows = max_nrows - ctx->row0;
                } else {
                  // We reached `allocnrow` limit, but there are more data to read
                  // left. In this case we arrange to terminate all threads but
                  // remember the position where the previous thread has finished. We
                  // will reallocate the DT and restart reading from the same point.
                  chunk0 = i;
                  if (i < nchunks - 1) {
                    extraAllocRows = (size_t)((double)(row0+ctx->used_nrows)*nchunks/(i+1) * 1.2) - allocnrow;
                    if (extraAllocRows < 1024) extraAllocRows = 1024;
                  } else {
                    // If we're on the last jump, then we know exactly how many extra rows is needed.
                    extraAllocRows = row0 + ctx->used_nrows - allocnrow;
                  }
                  ctx->used_nrows = 0; // do not push this chunk
                  chunkster->unorder_chunk(acc);
                  stopTeam = true;
                }
              }
              row0 += ctx->used_nrows;
              if (!stopTeam) ctx->orderBuffer();

            } catch (...) {
              oem.capture_exception();
              stopTeam = true;
            }
          } while (0);  // #pragma omp ordered
        }  // #pragma omp for ordered
        try {
          // Push out all buffers one last time.
          if (ctx->used_nrows) {
            if (stopTeam && (stopErr[0]!='\0' || oem.exception_caught())) {
              // Stopped early because of error. Discard the content of the buffers,
              // because they were not ordered, and trying to push them may lead to
              // unexpected bugs...
              ctx->used_nrows = 0;
            } else {
              ctx->push_buffers();
              thPush += ctx->thPush;
            }
          }
        } catch (...) {
          oem.capture_exception();
        }

        #pragma omp critical
        {
          nTypeBump += ctx->nTypeBump;
          progressShown |= tShowAlways;
        }
      }  // #pragma omp parallel

      if (progressShown) {
        int status = 1;
        if (oem.exception_caught()) {
          status++;
          try {
            oem.rethrow_exception_if_any();
          } catch (PyError& e) {
            status += e.is_keyboard_interrupt();
            oem.capture_exception();
          } catch (...) {
            oem.capture_exception();
          }
        }
        f.progress(chunkster->work_done_amount(), status);
      }
      oem.rethrow_exception_if_any();
      if (stopTeam && stopErr[0]!='\0') {
        STOP(stopErr);
      }
      ASSERT(row0 <= allocnrow || max_nrows <= allocnrow);
      if (extraAllocRows) {
        allocnrow += extraAllocRows;
        if (allocnrow > max_nrows) allocnrow = max_nrows;
        f.trace("  Too few rows allocated. Allocating additional %llu rows "
                "(now nrows=%llu) and continue reading from jump point %zu",
                (llu)extraAllocRows, (llu)allocnrow, chunk0);
        f.columns.allocate(allocnrow);
        read_all();
      } else {
        f.columns.allocate(row0);
        thRead /= nthreads;
        thPush /= nthreads;
      }
    }
};








//=================================================================================================
//
// Main fread() function that does all the job of reading a text/csv file.
//
// Returns 1 if it finishes successfully, and 0 otherwise.
//
//=================================================================================================
DataTablePtr FreadReader::read()
{
  double t0 = wallclock();

  const ParserFnPtr* parsers = parserlib.get_parser_fns();

  // Convenience variable for iterating over the file.
  const char* ch = NULL;

  // Test whether '\n's are present in the file at all... If not, then standalone '\r's are valid
  // line endings. However if '\n' exists in the file, then '\r' will be considered as regular
  // characters instead of a line ending.
  int cnt = 0;
  ch = sof;
  while (ch < eof && *ch != '\n' && cnt < 100) {
    cnt += (*ch == '\r');
    ch++;
  }
  LFpresent = (ch < eof && *ch == '\n');
  if (LFpresent) {
    trace("LF character (\\n) found in input, \\r-only line endings are prohibited");
  } else {
    trace("LF character (\\n) not found in input, CR (\\r) will be considered a line ending");
  }


  //*********************************************************************************************
  // [6] Auto detect separator, quoting rule, first line and ncols, simply,
  //     using jump 0 only.
  //
  //     Always sample as if nrows= wasn't supplied. That's probably *why*
  //     user is setting nrow=0 to get the column names and types, without
  //     actually reading the data yet. Most likely to check consistency
  //     across a set of files.
  //*********************************************************************************************
  const char* firstJumpEnd = NULL; // remember where the winning jumpline from jump 0 ends, to know its size excluding header
  {
    if (verbose) trace("[06] Detect separator, quoting rule, and ncolumns");

    int nseps;
    char seps[] = ",|;\t ";  // default seps in order of preference. See ?fread.
    // using seps[] not *seps for writeability (http://stackoverflow.com/a/164258/403310)

    if (sep == '\xFF') {   // '\xFF' means 'auto'
      nseps = (int) strlen(seps);
    } else {
      seps[0] = sep;
      seps[1] = '\0';
      nseps = 1;
      if (verbose) trace("  Using supplied sep '%s'", sep=='\t' ? "\\t" : seps);
    }

    int topNumLines=0;        // the most number of lines with the same number of fields, so far
    int topNumFields=1;       // how many fields that was, to resolve ties
    char topSep='\n';         // which sep that was, by default \n to mean single-column input (1 field)
    int8_t topQuoteRule=0;    // which quote rule that was
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
    for (int s=0; s<nseps; s++) {
      sep = seps[s];
      whiteChar = (sep==' ' ? '\t' : (sep=='\t' ? ' ' : 0));  // 0 means both ' ' and '\t' to be skipped
      for (quoteRule=0; quoteRule<4; quoteRule++) {  // quote rule in order of preference
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
        if (firstJumpEnd == NULL) firstJumpEnd = tch;  // if this wins (doesn't get updated), it'll be single column input
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
          }
        }
        if (verbose && updated) {
          trace(sep<' '? "  sep=%#02x with %d lines of %d fields using quote rule %d" :
                         "  sep='%c' with %d lines of %d fields using quote rule %d",
                sep, topNumLines, topNumFields, topQuoteRule);
        }
      }
    }
    ASSERT(firstJumpEnd);
    quoteRule = ctx.quoteRule = topQuoteRule;
    sep = ctx.sep = topSep;
    whiteChar = ctx.whiteChar = (sep==' ' ? '\t' : (sep=='\t' ? ' ' : 0));
    if (sep==' ' && !fill) {
      if (verbose) trace("  sep=' ' detected, setting fill to True\n");
      fill = 1;
    }

    // Find the first line with the consistent number of fields.  There might
    // be irregular header lines above it.
    const char* prevStart = NULL;  // the start of the non-empty line before the first not-ignored row
    int ncols;
    if (fill) {
      // start input from first populated line; do not alter sof.
      ncols = topNmax;
    } else {
      ncols = topNumFields;
      int thisLine = -1;
      tch = sof;
      while (tch < eof && ++thisLine < JUMPLINES) {
        const char* lastLineStart = tch;   // lineStart
        int cols = ctx.countfields();  // advances tch to next line
        if (cols == ncols) {
          tch = sof = lastLineStart;
          line += thisLine;
          break;
        } else {
          prevStart = (cols > 0)? lastLineStart : NULL;
        }
      }
    }
    ASSERT(ncols >= 1 && line >= 1);

    // Create vector of Column objects
    columns.reserve(static_cast<size_t>(ncols));
    for (int i = 0; i < ncols; i++) {
      columns.push_back(GReaderColumn());
    }

    // For standard regular separated files, we're now on the first byte of the file.
    tch = sof;
    int tt = ctx.countfields();
    tch = sof;  // move back to start of line since countfields() moved to next
    ASSERT(fill || tt == ncols);
    if (verbose) {
      trace("  Detected %d columns on line %d. This line is either column "
            "names or first data row. Line starts as: \"%s\"",
            tt, line, strlim(sof, 30));
      trace("  Quote rule picked = %d", quoteRule);
      if (fill) trace("  fill=true and the most number of columns found is %d", ncols);
    }

    // Now check previous line which is being discarded and give helpful message to user
    if (prevStart) {
      tch = prevStart;
      int ttt = ctx.countfields();
      ASSERT(ttt != ncols);
      ASSERT(tch==sof);
      if (ttt > 1) {
        warn("Starting data input on line %d <<%s>> with %d fields and discarding "
             "line %d <<%s>> before it because it has a different number of fields (%d).",
             line, strlim(sof, 30), ncols, line-1, strlim(prevStart, 30), ttt);
      }
    }
    ch = tch;
  }


  //*********************************************************************************************
  // [7] Detect column types, good nrow estimate and whether first row is column names.
  //     At the same time, calc mean and sd of row lengths in sample for very
  //     good nrow estimate.
  //*********************************************************************************************
  size_t nChunks;          // How many jumps to use when pre-scanning the file
  size_t sampleLines;     // How many lines were sampled during the initial pre-scan
  size_t bytesRead;       // Bytes in the whole data section
  const char* lastRowEnd; // Pointer to the end of the data section
  {
    if (verbose) trace("[07] Detect column types, and whether first row contains column names");
    size_t ncols = columns.size();

    int8_t type0 = 1;
    columns.setType(type0);
    field64 trash;
    FreadTokenizer fctx = makeTokenizer(&trash, nullptr);
    const char*& tch = fctx.ch;

    // the size in bytes of the first JUMPLINES from the start (jump point 0)
    size_t jump0size = (size_t)(firstJumpEnd - sof);
    // how many places in the file to jump to and test types there (the very end is added as 11th or 101th)
    // not too many though so as not to slow down wide files; e.g. 10,000 columns.  But for such large files (50GB) it is
    // worth spending a few extra seconds sampling 10,000 rows to decrease a chance of costly reread even further.
    nChunks = 0;
    size_t sz = (size_t)(eof - sof);
    if (jump0size>0) {
      if (jump0size*100*2 < sz) nChunks=100;  // 100 jumps * 100 lines = 10,000 line sample
      else if (jump0size*10*2 < sz) nChunks=10;
      // *2 to get a good spacing. We don't want overlaps resulting in double counting.
      // nChunks==1 means the whole (small) file will be sampled with one thread
    }
    nChunks++; // the extra sample at the very end (up to eof) is sampled and format checked but not jumped to when reading
    if (verbose) {
      if (jump0size==0)
        trace("  Number of sampling jump points = %d because jump0size==0", nChunks);
      else
        trace("  Number of sampling jump points = %d because (%zd bytes from row 1 to eof) / (2 * %zd jump0size) == %zd",
              nChunks, sz, jump0size, sz/(2*jump0size));
    }

    sampleLines = 0;
    int64_t row1Line = line;
    double sumLen = 0.0;
    double sumLenSq = 0.0;
    int minLen = INT32_MAX;   // int_max so the first if(thisLen<minLen) is always true; similarly for max
    int maxLen = -1;
    lastRowEnd = sof;
    bool firstDataRowAfterPotentialColumnNames = false;  // for test 1585.7
    bool lastSampleJumpOk = false;   // it won't be ok if its nextGoodLine returns false as testing in test 1768
    auto fco = std::unique_ptr<FreadChunkOrganizer>(new FreadChunkOrganizer(sof, eof, *this));
    for (size_t j = 0; j < nChunks; ++j) {
      tch = (j == 0) ? sof :
            (j == nChunks-1) ? eof - (size_t)(0.5*jump0size) :
                              sof + j * (sz/(nChunks-1));
      if (tch < lastRowEnd) tch = lastRowEnd;
      // Skip any potential newlines, in case we jumped in the middle of one.
      // In particular, it could be problematic if the file had '\n\r' newlines
      // and we jumped onto the second '\r' (which wouldn't be considered a
      // newline by `skip_eol()`s rules, which would then become a part of the
      // following field).
      while (*tch == '\n' || *tch == '\r') tch++;
      if (tch >= eof) break;
      if (j > 0) {
        ChunkCoordinates cc(tch, eof);
        // skip this jump for sampling. Very unusual and in such unusual cases, we don't mind a slightly worse guess.
        if (!fco->next_good_line_start(cc, fctx)) continue;
      }
      bool bumped = false;  // did this jump find any different types; to reduce verbose output to relevant lines
      bool skip = false;
      int jline = 0;  // line from this jump point

      while (tch < eof && (jline<JUMPLINES || j==nChunks-1)) {
        // nChunks==1 implies sample all of input to eof; last jump to eof too
        const char* jlineStart = tch;
        if (sep==' ') while (tch<eof && *tch==' ') tch++;  // multiple sep=' ' at the jlineStart does not mean sep(!)
        // detect blank lines
        fctx.skip_white();
        if (tch == eof) break;
        if (ncols > 1 && fctx.skip_eol()) {
          if (skip_blank_lines) continue;
          if (!fill) break;
          sampleLines++;
          lastRowEnd = tch;
          continue;
        }
        jline++;
        size_t field = 0;
        const char* fieldStart = NULL;  // Needed outside loop for error messages below
        tch--;
        while (field < ncols) {
          tch++;
          fctx.skip_white();
          fieldStart = tch;
          bool thisColumnNameWasString = false;
          if (firstDataRowAfterPotentialColumnNames) {
            // 2nd non-blank row is being read now.
            // 1st row's type is remembered and compared (a little lower down)
            // to second row to decide if 1st row is column names or not
            thisColumnNameWasString = columns[field].isstring();
            columns[field].type = type0;  // re-initialize for 2nd row onwards
          }
          while (true) {
            parsers[columns[field].type](fctx);
            fctx.skip_white();
            if (fctx.end_of_field()) break;
            tch = fctx.end_NA_string(fieldStart);
            fctx.skip_white();
            if (fctx.end_of_field()) break;
            if (!columns[field].isstring()) {
              tch = fieldStart;
              if (*tch==quote) {
                tch++;
                parsers[columns[field].type](fctx);
                if (*tch==quote) {
                  tch++;
                  fctx.skip_white();
                  if (fctx.end_of_field()) break;
                }
              }
              columns[field].type++;
            } else {
              // the field could not be read with this quote rule, try again with next one
              // Trying the next rule will only be successful if the number of fields is consistent with it
              ASSERT(quoteRule < 3);
              if (verbose)
                trace("Bumping quote rule from %d to %d due to field %d on line %d of sampling jump %d starting \"%s\"",
                      quoteRule, quoteRule+1, field+1, jline, j, strlim(fieldStart,200));
              quoteRule++;
              fctx.quoteRule++;
            }
            bumped = true;
            tch = fieldStart;
          }
          if (ISNA<int8_t>(header) && thisColumnNameWasString && !columns[field].isstring()) {
            header = true;
            trace("header determined to be True due to column %d containing a string on row 1 and a lower type (%s) on row 2",
                  field + 1, columns[field].typeName());
          }
          if (*tch!=sep || *tch=='\n' || *tch=='\r') break;
          if (sep==' ') {
            while (tch[1]==' ') tch++;
            if (tch[1]=='\r' || tch[1]=='\n' || tch[1]=='\0') { tch++; break; }
          }
          field++;
        }
        bool eol_found = fctx.skip_eol();
        if (field < ncols-1 && !fill) {
          ASSERT(tch==eof || eol_found);
          STOP("Line %d has too few fields when detecting types. Use fill=True to pad with NA. "
               "Expecting %d fields but found %d: \"%s\"", jline, ncols, field+1, strlim(jlineStart, 200));
        }
        if (field>=ncols || !(eol_found || tch==eof)) {   // >=ncols covers ==ncols. We do not expect >ncols to ever happen.
          if (j==0) {
            STOP("Line %d starting <<%s>> has more than the expected %d fields. "
               "Separator '%c' occurs at position %d which is character %d of the last field: <<%s>>. "
               "Consider setting 'comment.char=' if there is a trailing comment to be ignored.",
               jline, strlim(jlineStart,10), ncols, *tch, (int)(tch-jlineStart+1), (int)(tch-fieldStart+1), strlim(fieldStart,200));
          }
          trace("  Not using sample from jump %d. Looks like a complicated file where "
                "nextGoodLine could not establish the true line start.", j);
          skip = true;
          break;
        }
        if (firstDataRowAfterPotentialColumnNames) {
          if (fill) {
            for (size_t jj = field+1; jj < ncols; jj++) {
              columns[jj].type = type0;
            }
          }
          firstDataRowAfterPotentialColumnNames = false;
        } else if (sampleLines==0) {
          // To trigger 2nd row starting from type 1 again to compare to 1st row to decide if column names present
          firstDataRowAfterPotentialColumnNames = true;
        }

        lastRowEnd = tch;
        int thisLineLen = (int)(tch-jlineStart);  // tch is now on start of next line so this includes EOLLEN already
        ASSERT(thisLineLen >= 0);
        sampleLines++;
        sumLen += thisLineLen;
        sumLenSq += thisLineLen*thisLineLen;
        if (thisLineLen<minLen) minLen=thisLineLen;
        if (thisLineLen>maxLen) maxLen=thisLineLen;
      }
      if (skip) continue;
      if (j==nChunks-1) lastSampleJumpOk = true;
      if (verbose && (bumped || j==0 || j==nChunks-1)) {
        trace("  Type codes (jump %03d): %s  Quote rule %d", j, columns.printTypes(), quoteRule);
      }
    }
    if (lastSampleJumpOk) {
      while (tch < eof && isspace(*tch)) tch++;
      if (tch < eof) {
        warn("Found the last consistent line but text exists afterwards (discarded): \"%s\"", strlim(tch, 200));
      }
    } else {
      // nextGoodLine() was false for the last (extra) jump to check the end
      // must set lastRowEnd to eof accordingly otherwise it'll be left wherever the last good jump finished
      lastRowEnd = eof;
    }
    eof = lastRowEnd;

    size_t estnrow = 1;
    allocnrow = 1;
    meanLineLen = 0;
    bytesRead = 0;

    if (ISNA<int8_t>(header)) {
      header = true;
      for (size_t j = 0; j < ncols; j++) {
        if (!columns[j].isstring()) {
          header = false;
          break;
        }
      }
      if (verbose) {
        trace("header detetected to be %s because %s",
              header? "True" : "False",
              sampleLines <= 1 ?
                (header? "there are numeric fields in the first and only row" :
                         "all fields in the first and only row are of string type") :
                (header? "all columns are of string type, and a better guess is not possible" :
                         "there are some columns containing only numeric data (even in the first row)"));
      }
    }

    if (sampleLines <= 1) {
      if (header == 1) {
        // A single-row input, and that row is the header. Reset all types to
        // boolean (lowest type possible, a better guess than "string").
        for (size_t j = 0; j < ncols; j++) {
          columns[j].type = type0;
        }
        allocnrow = 0;
      }
      meanLineLen = sumLen;
    } else {
      bytesRead = (size_t)(lastRowEnd - sof);
      meanLineLen = sumLen/sampleLines;
      estnrow = static_cast<size_t>(std::ceil(bytesRead/meanLineLen));  // only used for progress meter and verbose line below
      double sd = std::sqrt( (sumLenSq - (sumLen*sumLen)/sampleLines)/(sampleLines-1) );
      allocnrow = std::max(static_cast<size_t>(bytesRead / fmax(meanLineLen - 2*sd, minLen)),
                           static_cast<size_t>(1.1*estnrow));
      allocnrow = std::min(allocnrow, 2*estnrow);
      // sd can be very close to 0.0 sometimes, so apply a +10% minimum
      // blank lines have length 1 so for fill=true apply a +100% maximum. It'll be grown if needed.
      if (verbose) {
        trace("  =====");
        trace("  Sampled %zd rows (handled \\n inside quoted fields) at %d jump point(s)", sampleLines, nChunks);
        trace("  Bytes from first data row on line %lld to the end of last row: %zd", row1Line, bytesRead);
        trace("  Line length: mean=%.2f sd=%.2f min=%d max=%d", meanLineLen, sd, minLen, maxLen);
        trace("  Estimated number of rows: %zd / %.2f = %zd", bytesRead, meanLineLen, estnrow);
        trace("  Initial alloc = %zd rows (%zd + %d%%) using bytes/max(mean-2*sd,min) clamped between [1.1*estn, 2.0*estn]",
              allocnrow, estnrow, (int)(100.0*allocnrow/estnrow-100.0));
      }
      if (nChunks==1) {
        if (header == 1) sampleLines--;
        estnrow = allocnrow = sampleLines;
        trace("All rows were sampled since file is small so we know nrow=%zd exactly", estnrow);
      } else {
        ASSERT(sampleLines <= allocnrow);
      }
      if (max_nrows < allocnrow) {
        trace("Alloc limited to nrows=%zd according to the provided max_nrows argument.", max_nrows);
        estnrow = allocnrow = max_nrows;
      }
      trace("=====");
    }
    ch = tch;
  }


  //*********************************************************************************************
  // [8] Parse column names (if present)
  //
  //     This section also moves the `sof` pointer to point at the first row
  //     of data ("removing" the column names).
  //*********************************************************************************************
  if (header == 1) {
    trace("[08] Assign column names");
    field64 tmp;
    FreadTokenizer fctx = makeTokenizer(&tmp, /* anchor= */ sof);
    fctx.ch = sof;
    parse_column_names(fctx);
    sof = fctx.ch;  // Update sof to point to the first line after the columns
  }


  //*********************************************************************************************
  // [9] Allow user to override column types; then allocate the DataTable
  //*********************************************************************************************
  double tLayout = wallclock();
  double tColType;    // Timer for applying user column class overrides
  double tAlloc;      // Timer for allocating the DataTable
  size_t rowSize;
  {
    if (verbose) trace("[09] Apply user overrides on column types");
    std::unique_ptr<int8_t[]> oldtypes = columns.getTypes();

    userOverride();

    size_t ncols = columns.size();
    size_t ndropped = 0;
    int nUserBumped = 0;
    rowSize = 0;
    for (size_t i = 0; i < ncols; i++) {
      GReaderColumn& col = columns[i];
      if (col.type == static_cast<int8_t>(PT::Drop)) {
        ndropped++;
        col.presentInOutput = false;
        col.presentInBuffer = false;
        continue;
      } else {
        rowSize += 8;
        if (col.type < oldtypes[i]) {
          // FIXME: if the user wants to override the type, let them
          STOP("Attempt to override column %d \"%s\" of inherent type '%s' down to '%s' which will lose accuracy. " \
               "If this was intended, please coerce to the lower type afterwards. Only overrides to a higher type are permitted.",
               i+1, col.name.data(), ParserLibrary::info(oldtypes[i]).cname(), col.typeName());
        }
        nUserBumped += (col.type != oldtypes[i]);
      }
    }
    if (verbose) {
      trace("  After %d type and %d drop user overrides : %s",
            nUserBumped, ndropped, columns.printTypes());
    }
    tColType = wallclock();

    if (verbose) {
      trace("  Allocating %d column slots (%d - %d dropped) with %zd rows",
            ncols-ndropped, ncols, ndropped, allocnrow);
    }

    columns.allocate(allocnrow);

    tAlloc = wallclock();
  }


  //*********************************************************************************************
  // [11] Read the data
  //*********************************************************************************************
  bool firstTime = true;
  int nTypeBump = 0;
  int nTypeBumpCols = 0;
  double tRead = 0.0;
  double tReread = 0.0;
  double thRead = 0.0;
  double thPush = 0.0;  // reductions of timings within the parallel region
  char* typeBumpMsg = NULL;
  size_t typeBumpMsgSize = 0;
  int typeCounts[ParserLibrary::num_parsers];  // used for verbose output
  int buffGrown = 0;

  std::unique_ptr<int8_t[]> typesPtr = columns.getTypes();
  int8_t* types = typesPtr.get();  // This pointer is valid until `typesPtr` goes out of scope
  if (sep == '\n') sep = '\xFF';

  read:  // we'll return here to reread any columns with out-of-sample type exceptions
  {
    trace("[11] Read the data");
    FreadChunkedReader scr(*this, rowSize, lastRowEnd,
                           typeBumpMsg, typeBumpMsgSize, types,
                           allocnrow);
    scr.read_all();
    thRead += scr.thRead;
    thPush += scr.thPush;
    buffGrown += scr.buffGrown;
    typeBumpMsg = scr.typeBumpMsg;
    typeBumpMsgSize = scr.typeBumpMsgSize;
    nTypeBump += scr.nTypeBump;
    nTypeBumpCols += scr.nTypeBumpCols;
    allocnrow = scr.allocnrow;
  }

  if (firstTime) {
    tReread = tRead = wallclock();
    size_t ncols = columns.size();

    for (size_t i = 0; i < ParserLibrary::num_parsers; ++i) typeCounts[i] = 0;
    for (size_t i = 0; i < ncols; i++) {
      typeCounts[columns[i].type]++;
    }

    if (nTypeBump) {
      rowSize = 0;
      for (size_t j = 0; j < ncols; j++) {
        GReaderColumn& col = columns[j];
        if (!col.presentInOutput) continue;
        if (col.typeBumped) {
          // column was bumped due to out-of-sample type exception
          col.typeBumped = false;
          col.presentInBuffer = true;
          rowSize += 8;
        } else {
          types[j] = static_cast<int8_t>(PT::Drop);
          col.presentInBuffer = false;
        }
      }
      firstTime = false;
      nTypeBump = 0;
      goto read;
    }
  } else {
    tReread = wallclock();
  }

  double tTot = tReread-t0;  // tReread==tRead when there was no reread
  trace("Read %zu rows x %d columns from %s file in %02d:%06.3f wall clock time",
        columns.nrows(), columns.nOutputs(), filesize_to_str(datasize()),
        (int)tTot/60, fmod(tTot,60.0));



  //*********************************************************************************************
  // [12] Finalize the datatable
  //*********************************************************************************************
  trace("[12] Finalizing the datatable");

  if (verbose) {
    size_t totalAllocSize = columns.totalAllocSize();
    size_t nrows = columns.nrows();
    trace("=============================");
    if (tTot < 0.000001) tTot = 0.000001;  // to avoid nan% output in some trivially small tests where tot==0.000s
    trace("%8.3fs (%3.0f%%) sep, ncols and header detection", tLayout-t0, 100.0*(tLayout-t0)/tTot);
    trace("%8.3fs (%3.0f%%) Column type detection using %zd sample rows",
          tColType-tLayout, 100.0*(tColType-tLayout)/tTot, sampleLines);
    trace("%8.3fs (%3.0f%%) Allocation of %llu rows x %d cols (%.3fGB) of which %llu (%3.0f%%) rows used",
          tAlloc-tColType, 100.0*(tAlloc-tColType)/tTot, (llu)allocnrow, columns.size(),
          totalAllocSize/(1024.0*1024*1024), (llu)nrows, 100.0*nrows/allocnrow);
    double thWaiting = tReread - tAlloc - thRead - thPush;
    trace("%8.3fs (%3.0f%%) Reading data", tReread-tAlloc, 100.0*(tReread-tAlloc)/tTot);
    trace("   + %8.3fs (%3.0f%%) Parse to row-major thread buffers (grown %d times)",
          thRead, 100.0*thRead/tTot, buffGrown);
    trace("   + %8.3fs (%3.0f%%) Transpose", thPush, 100.0*thPush/tTot);
    trace("   + %8.3fs (%3.0f%%) Waiting", thWaiting, 100.0*thWaiting/tTot);
    trace("%8.3fs (%3.0f%%) Rereading %d columns due to out-of-sample type exceptions",
          tReread-tRead, 100.0*(tReread-tRead)/tTot, nTypeBumpCols);
    trace("%8.3fs        Total", tTot);
    if (typeBumpMsg) {
      // if type bumps happened, it's useful to see them at the end after the timing 2 lines up showing the reread time
      trace("%s", typeBumpMsg);
      free(typeBumpMsg);  // local scope and only populated in verbose mode
    }
  }

  return makeDatatable();
}
