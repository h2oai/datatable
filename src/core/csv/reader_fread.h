//------------------------------------------------------------------------------
// Copyright 2018-2020 H2O.ai
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
//------------------------------------------------------------------------------
#ifndef dt_CSV_READER_FREAD_h
#define dt_CSV_READER_FREAD_h
#include <vector>                 // std::vector
#include "csv/reader.h"           // GenericReader
#include "csv/reader_parsers.h"   // ParserLibrary, ParserFnPtr
#include "read/field64.h"         // dt::read::field64
#include "parallel/atomic.h"      // dt::atomic

namespace dt {
namespace read {
  class ParallelReader;
  class FreadParallelReader;
  class FreadThreadContext;
}}
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
    const dt::read::GenericReader& g;
    double t_start;
    double t_initialized;
    double t_parse_parameters_detected;
    double t_column_types_detected;
    double t_frame_allocated;
    double t_data_read;
    dt::atomic<double> time_read_data;
    dt::atomic<double> time_push_data;
    size_t n_rows_read;
    size_t n_cols_read;
    size_t input_size;
    size_t n_lines_sampled;
    size_t n_rows_allocated;
    size_t n_cols_allocated;
    size_t allocation_size;
    size_t read_data_nthreads;
    std::vector<std::string> messages;

  public:
    FreadObserver(const dt::read::GenericReader&);
    ~FreadObserver();

    void type_bump_info(size_t icol, const dt::read::InputColumn& col, dt::read::PT new_type,
                        const char* field, int64_t len, int64_t lineno);

    void report();
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
class FreadReader : public dt::read::GenericReader
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
  size_t n_sample_lines;
  bool reread_scheduled;

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
  int64_t : 40;

public:
  explicit FreadReader(const GenericReader&);
  virtual ~FreadReader() override;

  std::unique_ptr<DataTable> read_all();

  // Simple getters
  double get_mean_line_len() const { return meanLineLen; }
  size_t get_ncols() const { return preframe.ncols(); }

  dt::read::ParseContext makeTokenizer() const;

private:
  void parse_column_names(dt::read::ParseContext& ctx);
  void detect_sep(dt::read::ParseContext& ctx);

  void detect_lf();
  void skip_preamble();
  void detect_sep_and_qr();
  void detect_column_types();
  void detect_header();
  int64_t parse_single_line(dt::read::ParseContext&);

  friend dt::read::FreadThreadContext;
  friend dt::read::FreadParallelReader;
  friend dt::read::ParallelReader;
  friend ColumnTypeDetectionChunkster;
};




#endif
