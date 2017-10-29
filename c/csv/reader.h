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
#include "memorybuf.h"


class GenericReader
{
  // Input parameters
  int nthreads;
  bool verbose;
  // Runtime parameters
  MemoryBuffer* mbuf;

public:
  GenericReader(PyObject* pyreader);
  ~GenericReader();

  bool read();
};



PyObject* gread(PyObject*, PyObject*);

#endif
