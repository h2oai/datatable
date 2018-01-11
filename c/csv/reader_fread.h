//------------------------------------------------------------------------------
//  Copyright 2017 H2O.ai
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.
//------------------------------------------------------------------------------
#ifndef dt_FREAD_IMPL_H
#define dt_FREAD_IMPL_H
#include "csv/reader.h"
#include <Python.h>
#include "csv/py_csv.h"
#include "csv/fread.h"
#include "memorybuf.h"

struct FreadLocalParseContext;



//------------------------------------------------------------------------------

/**
 * Fast parallel reading of CSV files with intelligent guessing of parse
 * parameters.
 *
 * This is a wrapper around freadMain function from R's data.table.
 */
class FreadReader
{
  GenericReader& g;
  RelStr* colNames;
  char* lineCopy;

  //----- Runtime parameters ---------------------------------------------------
  // ncols: number of fields in the CSV file. This field first becomes available
  //     in the `userOverride()` callback, and doesn't change after that.
  // nstrcols: number of string columns in the output DataTable. This will be
  //     computed within `allocateDT()` callback, and used for allocation of
  //     string buffers. If the file is re-read (due to type bumps), this
  //     variable will only count those string columns that need to be re-read.
  // ndigits: len(str(ncols))
  // types: array of types for each field in the input file, length `ncols`.
  //     Borrowed ref, do not free.
  // sizes: array of byte sizes for each field, length `ncols`.
  //     Borrowed ref, do not free.
  char* targetdir;
  StrBuf** strbufs;
  DataTablePtr dt;
  int ncols;
  int nstrcols;
  int ndigits;
  int: 32;
  int8_t* types;
  int8_t* sizes;
  int8_t* tmpTypes;
  const char* eof;

  //----- Parse parameters -----------------------------------------------------
  // quoteRule:
  //   0 = Fields may be quoted, any quote inside the field is doubled. This is
  //       the CSV standard. For example: <<...,"hello ""world""",...>>
  //   1 = Fields may be quoted, any quotes inside are escaped with a backslash.
  //       For example: <<...,"hello \"world\"",...>>
  //   2 = Fields may be quoted, but any quotes inside will appear verbatim and
  //       not escaped in any way. It is not always possible to parse the file
  //       unambiguously, but we give it a try anyways. A quote will be presumed
  //       to mark the end of the field iff it is followed by the field
  //       separator. Under this rule EOL characters cannot appear inside the
  //       field. For example: <<...,"hello "world"",...>>
  //   3 = Fields are not quoted at all. Any quote characters appearing anywhere
  //       inside the field will be treated as any other regular characters.
  //       Example: <<...,hello "world",...>>
  //
  char whiteChar;
  char dec;
  char sep;
  char quote;
  int8_t quoteRule;
  bool stripWhite;
  bool blank_is_a_NAstring;
  bool LFpresent;

public:
  FreadReader(GenericReader&);
  ~FreadReader();
  DataTablePtr read();

private:
  int freadMain();
  FieldParseContext makeFieldParseContext(
      const char*& ch, field64* target, const char* anchor);

  /**
   * This callback is invoked by `freadMain` after the initial pre-scan of the
   * file, when all parsing parameters have been determined; most importantly the
   * column names and their types.
   *
   * This function serves two purposes: first, it tells the upstream code what the
   * detected column names are; and secondly what is the expected type of each
   * column. The upstream code then has an opportunity to upcast the column types
   * if requested by the user, or mark some columns as skipped.
   *
   * @param types
   *    type codes of each column in the CSV file. Possible type codes are
   *    described by the `colType` enum. The function may modify this array
   *    setting some types to 0 (CT_DROP), or upcasting the types. Downcasting is
   *    not allowed and will trigger an error from `freadMain` later on.
   *
   * @param anchor
   *    pointer to a string buffer (usually somewhere inside the memory-mapped
   *    file) within which the column names are located, as described by the
   *    `colNames` array.
   *
   * @param ncols
   *    total number of columns. This is the length of arrays `types` and
   *    `colNames`.
   */
  void userOverride(int8_t *types, const char* anchor, int ncols);

  /**
   * This function is invoked by `freadMain` right before the main scan of the
   * input file. This function should allocate the resulting `DataTable` structure
   * and prepare to receive the data in chunks.
   *
   * If the input file needs to be re-read due to out-of-sample type exceptions,
   * then this function will be called second time with updated `types` array.
   * Then this function's responsibility is to update the allocation of those
   * columns properly.
   *
   * @param ncols
   *    number of columns in the CSV file. This is the size of arrays `types` and
   *    `sizes`.
   *
   * @param ndrop
   *    count of columns with type CT_DROP. This parameter is provided for
   *    convenience, since it can always be computed from `types`. The resulting
   *    datatable will have `ncols - ndrop` columns.
   *
   * @param nrows
   *    the number of rows to allocate for the datatable. This number of rows is
   *    estimated during the initial pre-scan, and then adjusted upwards to
   *    account for possible variation. It is very unlikely that this number
   *    underestimates the final row count.
   *
   * @return
   *    this function should return the total size of the Datatable created (for
   *    reporting purposes). If the return value is 0, then it indicates an error
   *    and `fread` will abort.
   */
  size_t allocateDT(int ncols, int ndrop, size_t nrows);

  /**
   * Called at the end to specify what the actual number of rows in the datatable
   * was. The function should adjust the datatable, reallocing the buffers if
   * necessary.
   * If the input file needs to be rescanned due to some columns having wrong
   * column types, then this function will be called once after the file is
   * finished scanning but before any calls to `reallocColType()`, and then the
   * second time after the entire input file was scanned again.
   */
  void setFinalNrow(size_t nrows);

  Column* alloc_column(SType stype, size_t nrows, int j);
  Column* realloc_column(Column *col, SType stype, size_t nrows, int j);

  /**
   * Progress-reporting function.
   *
   * @param percent
   *    A number from 0 to 100
   */
  void progress(double percent);

  void prepareThreadContext(FreadLocalParseContext *ctx);
  void postprocessBuffer(FreadLocalParseContext *ctx);
  void orderBuffer(FreadLocalParseContext *ctx);
  void pushBuffer(FreadLocalParseContext *ctx);
  void freeThreadContext(FreadLocalParseContext *ctx);

  const char* printTypes(int ncol) const;
};



//------------------------------------------------------------------------------
// FreadLocalParseContext
//------------------------------------------------------------------------------

struct FreadLocalParseContext
{
  // Pointer that serves as a starting point for all offsets within the `RelStr`
  // structs.
  const char* anchor;

  // Output buffer. Within the buffer the data is stored in row-major order,
  // i.e. in the same order as in the original CSV file.
  field64*  buff;

  // Size (in bytes) for a single row of data within the buffer. This should be
  // equal to `ncol * 8`.
  size_t rowSize;

  // Starting row index within the output DataTable for the current data chunk.
  size_t DTi;

  // Number of rows currently being stored within the buffers. The allocation
  // size of each `buffX` is thus at least `nRows * rowSizeX`.
  size_t nRows;

  // Reference to the flag that controls the parser's execution. Setting this
  // flag to true will force parsing of the CSV file to terminate in the near
  // future.
  bool* stopTeam;

  int threadn;

  int quoteRule;

  char quote;

  int64_t : 56;

  // Any additional implementation-specific parameters.
  StrBuf* strbufs;
};



// Exception-raising macro for `fread()`, which renames it into "STOP". Usage:
//     if (cond) STOP("Bad things happened: %s", smth_bad);
//
#define STOP(...)                                                              \
    do {                                                                       \
        PyErr_Format(PyExc_RuntimeError, __VA_ARGS__);                         \
        throw PyError();                                                       \
    } while(0)


// This macro raises a warning using Python's standard warning mechanism. Usage:
//     if (cond) WARN("Attention: %s", smth_smelly);
//
#define DTWARN(...)                                                            \
    do {                                                                       \
        PyErr_WarnFormat(PyExc_RuntimeWarning, 1, __VA_ARGS__);                \
        if (warningsAreErrors) {                                               \
            return 0;                                                          \
        }                                                                      \
    } while(0)


#define DTPRINT(...) g.trace(__VA_ARGS__)

#endif
