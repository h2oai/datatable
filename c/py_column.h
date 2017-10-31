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

namespace pycolumn
{
#define BASECLS pycolumn::obj
#define HOMEFLAG PY_COLUMN_cc


typedef struct obj {
    PyObject_HEAD
    Column *ref;
    DataTable_PyObject *pydt;
    int64_t colidx;
} obj;


extern PyTypeObject type;
extern PyBufferProcs as_buffer;
extern PyObject* fn_hexview;


pycolumn::obj* from_column(Column*, DataTable_PyObject*, int64_t);
int static_init(PyObject* module);



//---- Generic info ------------------------------------------------------------

DECLARE_INFO(
  "_datatable.Column",
  "Single Column within a DataTable.")


//---- Getters/setters ---------------------------------------------------------

DECLARE_GETTER(
  mtype,
  "'Memory' type of the column: data, or memmap")

DECLARE_GETTER(
  stype,
  "'Storage' type of the column")

DECLARE_GETTER(
  ltype,
  "'Logical' type of the column")

DECLARE_GETTER(
  data_size,
  "The amount of memory taken by column's data")

DECLARE_GETTER(
  data_pointer,
  "Pointer (cast to long int) to the column's internal memory buffer")

DECLARE_GETTER(
  refcount,
  "Reference count of the column")

DECLARE_GETTER(
  meta,
  "String representation of the column's `meta` struct")



//---- Methods -----------------------------------------------------------------

DECLARE_METHOD(
  save_to_disk,
  "save_to_disk(filename)\n\n"
  "Save this column's data into the file `filename`.\n")

DECLARE_METHOD(
  hexview,
  "hexview()\n\n"
  "View column's internal data as hex bytes.\n")



};  // namespace pycolumn
#undef BASECLS
#undef HOMEFLAG
#endif
