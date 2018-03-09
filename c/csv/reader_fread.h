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
class FreadChunkedReader;


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

  // Simple getters
  int get_nthreads() const { return g.nthreads; }
  double get_mean_line_len() const { return meanLineLen; }
  size_t get_ncols() const { return columns.size(); }
  bool get_fill() const { return g.fill; }
  bool get_skip_empty_lines() const { return g.skip_blank_lines; }

  FreadTokenizer makeTokenizer(field64* target, const char* anchor) const;

private:
  void parse_column_names(FreadTokenizer& ctx);
  void detect_sep(FreadTokenizer& ctx);
  void userOverride();
  void progress(double progress, int status = 0);
  DataTablePtr makeDatatable();

  friend FreadLocalParseContext;
  friend FreadChunkedReader;
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
    char sep;
    bool verbose;
    bool fill;
    bool skipEmptyLines;
    bool numbersMayBeNAs;
    int64_t : 16;
    int nTypeBump;
    double thPush;
    int8_t* types;

    FreadReader& freader;
    GReaderColumns& columns;
    std::vector<StrBuf> strbufs;
    FreadTokenizer tokenizer;
    ParserFnPtr* parsers;

    char*& typeBumpMsg;
    size_t& typeBumpMsgSize;
    char*& stopErr;
    size_t& stopErrSize;

  public:
    FreadLocalParseContext(size_t bcols, size_t brows, FreadReader&, int8_t*,
                           ParserFnPtr* parsers, char*& typeBumpMsg,
                           size_t& typeBumpMsgSize, char*& stopErr,
                           size_t& stopErrSize, bool fill);
    FreadLocalParseContext(const FreadLocalParseContext&) = delete;
    FreadLocalParseContext& operator=(const FreadLocalParseContext&) = delete;
    virtual ~FreadLocalParseContext();
    virtual void push_buffers() override;
    ChunkCoordinates read_chunk(const ChunkCoordinates&) override;
    void postprocess();
    void orderBuffer() override;
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



#define ASSERT(test) do { \
  if (!(test)) \
    STOP("Assertion violation at line %d, please report", __LINE__); \
} while(0)


#endif
