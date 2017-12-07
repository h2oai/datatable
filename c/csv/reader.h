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
#include "column.h"
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
  public:
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
    bool warnings_to_errors;
    int: 16;

  // Runtime parameters
  private:
    PyObj logger;
    PyObj freader;
    PyObj src_arg;
    PyObj file_arg;
    PyObj text_arg;
    PyObj skipstring_arg;
    PyObj tempstr;
    MemoryBuffer* mbuf;
    size_t offset;
    int32_t fileno;
    int : 32;

  // Public API
  public:
    GenericReader(const PyObj& pyreader);
    ~GenericReader();

    DataTablePtr read();

    const char* dataptr() const;
    size_t datasize() const;
    const PyObj& pyreader() const { return freader; }
    bool get_verbose() const { return verbose; }
    void trace(const char* format, ...) const;

  // Helper functions
  private:
    void set_nthreads(int32_t nthreads);
    void set_verbose(int8_t verbose);
    void set_fill(int8_t fill);
    void set_maxnrows(int64_t n);
    void set_skiplines(int64_t n);

    void open_input();
    void detect_and_skip_bom();
    void skip_initial_whitespace();
    void decode_utf16();

    DataTablePtr read_empty_input();
    void detect_improper_files();
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
