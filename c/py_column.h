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
#ifndef dt_PYCOLUMN_H
#define dt_PYCOLUMN_H
#include <Python.h>
#include "column.h"
#include "py_datatable.h"
#include "py_utils.h"


typedef struct Column_PyObject {
    PyObject_HEAD
    Column *ref;
    DataTable_PyObject *pydt;
    int64_t colidx;
} Column_PyObject;


extern PyTypeObject Column_PyType;
extern PyBufferProcs column_as_buffer;
extern PyObject* pyfn_column_hexview;


Column_PyObject* pycolumn_from_column(
    Column*, DataTable_PyObject*, int64_t colidx);
int init_py_column(PyObject *module);



//---- Getters/setters ---------------------------------------------------------
#define PYCOLUMN_GETTER(fn, doc) \
  DECLARE_GETTER(fn, Column_PyObject, doc, PY_COLUMN_cc)

PYCOLUMN_GETTER(
  mtype,
  "'Memory' type of the column: data, or memmap")

PYCOLUMN_GETTER(
  stype,
  "'Storage' type of the column")

PYCOLUMN_GETTER(
  ltype,
  "'Logical' type of the column")

PYCOLUMN_GETTER(
  data_size,
  "The amount of memory taken by column's data")

PYCOLUMN_GETTER(
  data_pointer,
  "Pointer (cast to long int) to the column's internal memory buffer")

PYCOLUMN_GETTER(
  refcount,
  "Reference count of the column")

PYCOLUMN_GETTER(
  meta,
  "String representation of the column's `meta` struct")



//---- Methods -----------------------------------------------------------------
#define PYCOLUMN_METHOD(fn, doc) \
  DECLARE_METHOD(fn, Column_PyObject, doc, PY_COLUMN_cc)

PYCOLUMN_METHOD(
  save_to_disk,
  "save_to_disk(filename)\n\n"
  "Save this column's data into the file `filename`.\n")

PYCOLUMN_METHOD(
  hexview,
  "hexview()\n\n"
  "View column's internal data as hex bytes.\n")


#endif
