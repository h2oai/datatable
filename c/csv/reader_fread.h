//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
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
class SimpleChunkedReader;


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
  // allocnrow:
  //     Number of rows in the allocated DataTable
  // meanLineLen:
  //     Average length (in bytes) of a single line in the input file
  GReaderColumns columns;
  char* targetdir;
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
  FieldParseContext makeFieldParseContext(field64* target, const char* anchor);
  void parse_column_names(FieldParseContext& ctx);
  void detect_sep(FieldParseContext& ctx);
  void userOverride();
  void progress(double percent);
  DataTablePtr makeDatatable();

  friend FreadLocalParseContext;
  friend SimpleChunkedReader;
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

    GReaderColumns& columns;
    std::vector<StrBuf> strbufs;

  public:
    FreadLocalParseContext(size_t bcols, size_t brows, FreadReader&);
    virtual ~FreadLocalParseContext();
    virtual void push_buffers() override;
    virtual const char* read_chunk(const char* start, const char* end) override;
    void postprocess();
    void orderBuffer();
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
