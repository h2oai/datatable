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

  ColumnSpec(std::string n, Type t): name(n), type(t) {}
};


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
  int line;        // line number currently being read
  std::vector<ColumnSpec> columns;

public:
  ArffReader(GenericReader&);
  ~ArffReader();

  std::unique_ptr<DataTable> read();

private:
  /**
   * Read the comment lines at the beginning of the file, and store them in
   * the `preamble` variable. This method is needed because ARFF files typically
   * carry extended description of the dataset in the initial comment section,
   * and the user may want to access that decsription.
   */
  void read_preamble();

  /**
   * Read the "\@relation <name>" expression from the input. If successful,
   * store the relation's name; otherwise the name will remain empty.
   */
  void read_relation();

  void read_attributes();

  /**
   * Returns true if `keyword` is present at the current location in the input.
   * The keyword can be an arbitrary string, and it is matched
   * case-insensitively. Both the keyword and the input are assumed to be
   * \0-terminated. The keyword cannot contain newlines.
   */
  bool read_keyword(const char* keyword);

  /**
   * Advances pointer `ch` to the next non-whitespace character on the current
   * line. Only spaces and tabs are considered whitespace. Returns true if any
   * whitespace was consumed.
   */
  bool read_whitespace();

  bool read_end_of_line();

  /**
   * Advances pointer `ch` to the next non-whitespace character, skipping over
   * regular whitespace, newlines and comments.
   */
  void skip_ext_whitespace();

  void skip_newlines();
};


#endif
