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
#ifndef CSV_PY_CSV_h
#define CSV_PY_CSV_h
#include <Python.h>
#include "py_utils.h"

// TODO: clean up these old-style declarations
PyObject* pywrite_csv(PyObject* self, PyObject* args);


DECLARE_FUNCTION(
  gread,
  "gread(reader)\n\n"
  "'Generic read' function, similar to `fread` but supports other file \n"
  "types, not just csv.\n",
  CSV_PY_CSV_cc)


#endif
