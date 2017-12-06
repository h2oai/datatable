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
#ifndef dt_CSV_READER_H
#define dt_CSV_READER_H
#include <Python.h>
#include <memory>        // std::unique_ptr
#include "csv/fread.h"
#include "datatable.h"
#include "memorybuf.h"
#include "utils/pyobj.h"

// Forward declarations
struct RelStr;
struct StrBuf2;
struct ColumnSpec;
struct OutputColumn;


//------------------------------------------------------------------------------

/**
 * GenericReader: base class for reading text files in multiple formats. This
 * class attempts to parse different formats in turn, until it succeeds.
 */
class GenericReader
{
  // Input parameters
  int32_t nthreads;
  bool verbose;
  char sep;
  char dec;
  char quote;
  int64_t max_nrows;
  int64_t skip_lines;
  const char* skip_string;
  const char* const* na_strings;
  int8_t header;
  bool strip_white;
  bool skip_blank_lines;
  bool show_progress;
  bool fill;
  int: 24;

  // Runtime parameters
  PyObj logger;
  PyObj freader;
  PyObj src_arg;
  PyObj file_arg;
  PyObj text_arg;
  PyObj skipstring_arg;
  MemoryBuffer* mbuf;
  int32_t fileno;
  int : 32;

public:
  GenericReader(const PyObj& pyreader);
  ~GenericReader();

  std::unique_ptr<DataTable> read();

  const char* dataptr() const;
  bool get_verbose() const { return verbose; }
  void trace(const char* format, ...) const;

private:
  void set_nthreads(int32_t nthreads);
  void set_verbose(int8_t verbose);
  void set_fill(int8_t fill);
  void set_maxnrows(int64_t n);
  void set_skiplines(int64_t n);
  void open_input();
  friend class FreadReader;
};



/**
 * Reader for files in ARFF format.
 */
class ArffReader
{
  GenericReader& greader;
  std::string preamble;
  std::string name;
  bool verbose;
  int64_t : 56;

  // Runtime parameters
  const char* ch;  // pointer to the current reading location
  int line;        // current line number within the input (1-based)
  int : 32;
  std::vector<ColumnSpec> columns;

public:
  ArffReader(GenericReader&);
  ~ArffReader();

  std::unique_ptr<DataTable> read();

private:
  // Read the comment lines at the beginning of the file, and store them in
  // the `preamble` variable. This method is needed because ARFF files typically
  // carry extended description of the dataset in the initial comment section,
  // and the user may want to have access to that decsription.
  void read_preamble();

  // Read the "\@relation <name>" expression from the input. If successful,
  // store the relation's name; otherwise the name will remain empty.
  void read_relation();

  void read_attributes();

  void read_data_decl();

  // Returns true if `keyword` is present at the current location in the input.
  // The keyword can be an arbitrary string, and it is matched
  // case-insensitively. Both the keyword and the input are assumed to be
  // \0-terminated. The keyword cannot contain newlines.
  bool read_keyword(const char* keyword);

  // Advances pointer `ch` to the next non-whitespace character on the current
  // line. Only spaces and tabs are considered whitespace. Returns true if any
  // whitespace was consumed.
  bool read_whitespace();

  bool read_end_of_line();

  // Advances pointer `ch` to the next non-whitespace character, skipping over
  // regular whitespace, newlines and comments.
  void skip_ext_whitespace();

  void skip_newlines();
};



/**
 * Wrapper class around `freadMain` function.
 */
class FreadReader {
  GenericReader& g;
  freadMainArgs frargs;

  // runtime parameters
  PyObj tempstr;

public:
  FreadReader(GenericReader& g);
  ~FreadReader();
  std::unique_ptr<DataTable> read();

private:
  int freadMain();

  int makeEmptyDT();
  void decode_utf16();

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
   * @param colNames
   *    array of `lenOff` structures (offsets are relative to the `anchor`)
   *    describing the column names. If the CSV file had no header row, then this
   *    array will be filled with 0s.
   *
   * @param anchor
   *    pointer to a string buffer (usually somewhere inside the memory-mapped
   *    file) within which the column names are located, as described by the
   *    `colNames` array.
   *
   * @param ncols
   *    total number of columns. This is the length of arrays `types` and
   *    `colNames`.
   *
   * @return
   *    this function may return `false` to request that fread abort reading
   *    the CSV file. Normally, this function should return `true`.
   */
  bool userOverride(int8_t *types, lenOff* colNames, const char* anchor, int ncols);

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
   * @param types
   *     array of type codes for each column. Same as in the `userOverride`
   *     function.
   *
   * @param sizes
   *    the size (in bytes) of each column within the buffer(s) that will be
   *    passed to `pushBuffer()` during the scan. This array should be saved for
   *    later use. It exists mostly for convenience, since the size of each
   *    non-skipped column may be determined from that column's type.
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
  size_t allocateDT(int8_t* types, int8_t* sizes, int ncols, int ndrop, size_t nrows);

  /**
   * Progress-reporting function.
   *
   * @param percent
   *    A number from 0 to 100
   */
  void progress(double percent);

  void DTPRINT(const char *format, ...);
};


//------------------------------------------------------------------------------
// Helper classes
//------------------------------------------------------------------------------

struct ColumnSpec {
  enum class Type: int8_t {
    Drop,
    Bool,
    Integer,
    Real,
    String
  };

  std::string name;
  Type type;
  int64_t : 56;

  ColumnSpec(std::string n, Type t): name(n), type(t) {}
};


/**
 * Per-column per-thread temporary string buffers used to assemble processed
 * string data. This buffer is used as a "staging ground" where the string data
 * is being stored / postprocessed before being transferred to the "main"
 * string buffer in a Column. Such 2-stage process is needed for the multi-
 * threaded string data writing.
 *
 * Members of this struct:
 *   .strdata -- memory region where the string data is stored.
 *   .allocsize -- allocation size of this memory buffer.
 *   .usedsize -- amount of memory already in use in the buffer.
 *   .writepos -- position in the global string data buffer where the current
 *       buffer's data should be moved. This value is returned from
 *       `WritableBuffer::prep_write()`.
 */
struct StrBuf2 {
  char* strdata;
  size_t allocsize;
  size_t usedsize;
  size_t writepos;
  int64_t colidx;

  StrBuf2(int64_t i);
  ~StrBuf2();
  void resize(size_t newsize);
};


struct OutputColumn {
  MemoryMemBuf* data;
  WritableBuffer* strdata;

  OutputColumn() : data(nullptr), strdata(nullptr) {}
  ~OutputColumn() {
    data->release();
    delete strdata;
  }
};


struct ThreadContext {
  void* wbuf;
  std::vector<StrBuf2> strbufs;
  size_t rowsize;
  size_t wbuf_nrows;
  size_t used_nrows;
  int ithread;
  int : 32;

  ThreadContext(int ithread, size_t nrows, size_t ncols);
  virtual ~ThreadContext();
  virtual void prepare_strbufs(const std::vector<ColumnSpec>& columns);
  virtual void* next_row();
  virtual void push_buffers() = 0;
  virtual void discard();
  virtual void order() = 0;
};


/**
 * "Relative string": a string defined as an offset+length relative to some
 * anchor point (which has to be provided separately). This is the internal data
 * structure for reading strings from a file.
 */
struct RelStr {
  int32_t offset;
  int32_t length;
};



#endif
