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
#include <Python.h>
#include <vector>        // std::vector
#include "csv/fread.h"
#include "csv/reader.h"
#include "csv/py_csv.h"
#include "memorybuf.h"

class FreadLocalParseContext;



//------------------------------------------------------------------------------
// FreadReader
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

  //----- Runtime parameters ---------------------------------------------------
  // nstrcols: number of string columns in the output DataTable. This will be
  //     computed within `allocateDT()` callback, and used for allocation of
  //     string buffers. If the file is re-read (due to type bumps), this
  //     variable will only count those string columns that need to be re-read.
  // ndigits: len(str(ncols))
  // sizes: array of byte sizes for each field, length `ncols`.
  //     Borrowed ref, do not free.
  // allocnrow:
  //     Number of rows in the allocated DataTable
  // meanLineLen:
  //     Average length (in bytes) of a single line in the input file
  GReaderColumns columns;
  char* targetdir;
  StrBuf** strbufs;
  DataTablePtr dt;
  int nstrcols;
  int ndigits;
  int8_t* sizes;
  const char* eof;
  size_t allocnrow;
  double meanLineLen;

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

  void parse_column_names(FieldParseContext& ctx);

  /**
   * This callback is invoked by `freadMain` after the initial pre-scan of the
   * file, when all parsing parameters have been determined; most importantly the
   * column names and their types.
   *
   * This function serves two purposes: first, it tells the upstream code what the
   * detected column names are; and secondly what is the expected type of each
   * column. The upstream code then has an opportunity to upcast the column types
   * if requested by the user, or mark some columns as skipped.
   */
  void userOverride();

  /**
   * This function is invoked by `freadMain` right before the main scan of the
   * input file. This function should allocate the resulting `DataTable` structure
   * and prepare to receive the data in chunks.
   *
   * If the input file needs to be re-read due to out-of-sample type exceptions,
   * then this function will be called second time with updated `types` array.
   * Then this function's responsibility is to update the allocation of those
   * columns properly.
   */
  size_t allocateDT();

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

  friend FreadLocalParseContext;
};



//------------------------------------------------------------------------------
// FreadLocalParseContext
//------------------------------------------------------------------------------

/**
 * anchor
 *   Pointer that serves as a starting point for all offsets in "RelStr" fields.
 *
 */
class FreadLocalParseContext : public LocalParseContext
{
  public:
    const char* anchor;
    int quoteRule;
    char quote;
    int : 24;

    StrBuf* strbufs;
    int nstrcols;
    int : 32;

    // TODO: these should be replaced with a single reference to
    //       GReaderColumns
    int ncols;
    StrBuf**& ostrbufs;
    int8_t*& sizes;
    DataTablePtr& dt;
    GReaderColumns& columns;

  public:
    FreadLocalParseContext(size_t bcols, size_t brows, FreadReader&);
    virtual ~FreadLocalParseContext();
    virtual void push_buffers() override;
    virtual const char* read_chunk(const char* start, const char* end) override;
    void postprocess();
    void orderBuffer();
    void pushBuffer();
};




//------------------------------------------------------------------------------
// Old helper macros
//------------------------------------------------------------------------------

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


#define ASSERT(test) do { \
  if (!(test)) \
    STOP("Assertion violation at line %d, please report", __LINE__); \
} while(0)


#endif
