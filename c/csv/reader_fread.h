//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#ifndef dt_CSV_READER_FREAD_h
#define dt_CSV_READER_FREAD_h
#include <Python.h>
#include <vector>        // std::vector
#include "csv/fread.h"
#include "csv/reader.h"
#include "csv/reader_parsers.h"
#include "csv/py_csv.h"
#include "memorybuf.h"
#include "utils/shared_mutex.h"

class FreadLocalParseContext;
class FreadChunkedReader;
class ChunkedDataReader;
class ColumnTypeDetectionChunkster;


//------------------------------------------------------------------------------
// FreadObserver
//------------------------------------------------------------------------------

/**
 * Helper class to gather various stats about fread's inner workings, and then
 * present them in the form of a report.
 */
class FreadObserver {
  public:
    double t_start;
    double t_initialized;
    double t_parse_parameters_detected;
    double t_column_types_detected;
    double t_frame_allocated;
    double t_data_read;
    double t_data_reread;
    double time_read_data;
    double time_push_data;
    size_t n_rows_read;
    size_t n_cols_read;
    size_t input_size;
    size_t n_lines_sampled;
    size_t n_rows_allocated;
    size_t n_cols_allocated;
    size_t n_cols_reread;
    size_t allocation_size;
    size_t read_data_nthreads;
    std::vector<std::string> messages;

  public:
    FreadObserver();
    ~FreadObserver();

    void type_bump_info(size_t icol, const GReaderColumn& col, PT new_type,
                        const char* field, int64_t len, int64_t lineno);
    void str64_bump(size_t icol, const GReaderColumn& col);

    void report(const GenericReader&);
};




//------------------------------------------------------------------------------
// FreadReader
//------------------------------------------------------------------------------

/**
 * Fast parallel reading of CSV files with intelligent guessing of parse
 * parameters.
 *
 * This is a wrapper around freadMain function from R's data.table.
 */
class FreadReader : public GenericReader
{
  //----- Runtime parameters ---------------------------------------------------
  // allocnrow:
  //     Number of rows in the allocated DataTable
  // meanLineLen:
  //     Average length (in bytes) of a single line in the input file
  ParserLibrary parserlib;
  const ParserFnPtr* parsers;
  FreadObserver fo;
  char* targetdir;
  size_t allocnrow;
  double meanLineLen;
  size_t first_jump_size;

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
  int8_t quoteRule;
  bool LFpresent;
  int64_t : 40;

public:
  explicit FreadReader(const GenericReader&);
  ~FreadReader();

  DataTablePtr read();

  // Simple getters
  double get_mean_line_len() const { return meanLineLen; }
  size_t get_ncols() const { return columns.size(); }

  FreadTokenizer makeTokenizer(field64* target, const char* anchor) const;

private:
  void parse_column_names(FreadTokenizer& ctx);
  void detect_sep(FreadTokenizer& ctx);
  void userOverride();

  void detect_column_types();
  int64_t parse_single_line(FreadTokenizer&, bool* bumped);

  friend FreadLocalParseContext;
  friend FreadChunkedReader;
  friend ChunkedDataReader;
  friend ColumnTypeDetectionChunkster;
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
    int64_t : 48;
    double ttime_push;
    double ttime_read;
    PT* types;

    FreadReader& freader;
    GReaderColumns& columns;
    dt::shared_mutex& shmutex;
    std::vector<StrBuf> strbufs;
    FreadTokenizer tokenizer;
    const ParserFnPtr* parsers;

  public:
    FreadLocalParseContext(size_t bcols, size_t brows, FreadReader&, PT* types,
                           dt::shared_mutex&);
    FreadLocalParseContext(const FreadLocalParseContext&) = delete;
    FreadLocalParseContext& operator=(const FreadLocalParseContext&) = delete;
    virtual ~FreadLocalParseContext();

    virtual void push_buffers() override;
    void read_chunk(const ChunkCoordinates&, ChunkCoordinates&) override;
    void postprocess();
    void orderBuffer() override;
};





class FreadChunkedReader : public ChunkedDataReader {
  private:
    FreadReader& f;
    PT* types;

  public:
    FreadChunkedReader(FreadReader& reader, PT* types_)
      : ChunkedDataReader(reader, reader.get_mean_line_len()), f(reader)
    {
      types = types_;
    }
    virtual ~FreadChunkedReader() {}

    virtual void read_all() override {
      ChunkedDataReader::read_all();
      f.fo.read_data_nthreads = static_cast<size_t>(nthreads);
    }

    bool next_good_line_start(
      const ChunkCoordinates& cc, FreadTokenizer& tokenizer) const;

  protected:
    virtual std::unique_ptr<LocalParseContext> init_thread_context() override {
      size_t trows = std::max<size_t>(nrows_allocated / chunkCount, 4);
      size_t tcols = f.columns.nColumnsInBuffer();
      return std::unique_ptr<LocalParseContext>(
                new FreadLocalParseContext(tcols, trows, f, types, shmutex));
    }

    void adjust_chunk_coordinates(
      ChunkCoordinates& cc, LocalParseContext* ctx) const override;

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


#endif
