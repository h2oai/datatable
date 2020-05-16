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
#ifndef dt_CSV_READER_ARFF_h
#define dt_CSV_READER_ARFF_h
#include "csv/reader.h"
#include "datatable.h"


/**
 * Reader for files in ARFF format.
 */
class ArffReader
{
  dt::read::GenericReader& g;
  std::string preamble;
  std::string name;
  bool verbose;
  int64_t : 56;

  // Runtime parameters
  const char* ch;  // pointer to the current reading location
  int line;        // current line number within the input (1-based)
  int : 32;
  std::vector<dt::read::InputColumn> columns;

public:
  ArffReader(dt::read::GenericReader&);
  ~ArffReader();

  std::unique_ptr<DataTable> read_all();

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


#endif
