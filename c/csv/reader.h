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
#include "datatable.h"
#include "memorybuf.h"

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
  int nthreads;
  bool verbose;
  // Runtime parameters
  PyObj filename_arg;
  PyObj text_arg;
  MemoryBuffer* mbuf;

public:
  GenericReader(PyObject* pyreader);
  ~GenericReader();

  std::unique_ptr<DataTable> read();

  const char* dataptr() const;
  bool get_verbose() const { return verbose; }

private:
  static int normalize_nthreads(const PyObj&);
  void open_input();
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
