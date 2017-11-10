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



class ArffReader {
  GenericReader& greader;
  std::string preamble;
  std::string name;
  bool verbose;
  int64_t : 56;

public:
  ArffReader(GenericReader&);
  ~ArffReader();

  std::unique_ptr<DataTable> read();

private:
  void read_preamble(const char** pch);

  /**
   * Read the "\@relation <name>" expression from the input. If successful,
   * return true, advance pointer `pch`, and store the relation's name; or
   * return false otherwise.
   */
  bool read_relation(const char** pch);
};


#endif
