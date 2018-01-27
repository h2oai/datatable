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
#ifndef dt_PY_COLUMNSET_h
#define dt_PY_COLUMNSET_h
#include <Python.h>
#include "datatable.h"
#include "rowindex.h"
#include "py_utils.h"


#define BASECLS pycolumnset::obj
#define HOMEFLAG dt_PY_COLUMNSET_cc
namespace pycolumnset
{

struct obj : public PyObject {
  Column** columns;
  int64_t ncols;
};

extern PyTypeObject type;


//---- Generic info ------------------------------------------------------------

DECLARE_INFO(
  datatable.core.ColumnSet,
  "Array of columns that can be used to construct a DataTable.")



//---- External API ------------------------------------------------------------

DECLARE_FUNCTION(
  columns_from_slice,
  "columns_from_slice(dt, rowindex, start, count, step)\n\n"
  "Retrieve set of columns as a slice of columns in DataTable `dt`.\n",
  HOMEFLAG)

DECLARE_FUNCTION(
  columns_from_array,
  "columns_from_array(dt, rowindex, indices)\n\n"
  "Extract an array of columns at given indices from DataTable `dt`.\n",
  HOMEFLAG)

DECLARE_FUNCTION(
  columns_from_mixed,
  "columns_from_mixed()\n\n",
  HOMEFLAG)

DECLARE_FUNCTION(
  columns_from_columns,
  "columns_from_columns(cols)\n\n"
  "Create a ColumnSet from a Python list of columns.",
  HOMEFLAG)


int unwrap(PyObject* source, void* target);
int static_init(PyObject* module);

};
#undef BASECLS
#undef HOMEFLAG
#endif
