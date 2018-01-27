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
#ifndef dt_PY_DATATABLE_h
#define dt_PY_DATATABLE_h
#include <Python.h>
#include "datatable.h"
#include "py_utils.h"


#define BASECLS pydatatable::obj
#define HOMEFLAG dt_PY_DATATABLE_cc
namespace pydatatable
{

/**
 * Pythonic "handle" to a DataTable object.
 *
 * This object provides an interface through which Python can access the
 * underlying C DataTable (without encumbering the latter with Python-specific
 * elements).
 *
 * Parameters
 * ----------
 * ref
 *     Reference to the C `DataTable` object. The referenced object *is owned*,
 *     that is current object is responsible for deallocating the referenced
 *     datatable upon being garbage-collected.
 *
 */
struct obj : public PyObject {
  DataTable* ref;
  SType use_stype_for_buffers;
  int64_t : 56;
};

extern PyTypeObject type;
extern PyBufferProcs as_buffer;


// Internal helper functions
PyObject* wrap(DataTable* dt);
int unwrap(PyObject* source, DataTable** address);
int static_init(PyObject* module);


//---- Generic info ------------------------------------------------------------

DECLARE_INFO(
  datatable.core.DataTable,
  "C-side DataTable object.")



//---- Getters/setters ---------------------------------------------------------

DECLARE_GETTER(
  nrows,
  "Number of rows in the datatable")

DECLARE_GETTER(
  ncols,
  "Number of columns in the datatable")

DECLARE_GETTER(
  isview,
  "Is the datatable view or now?")

DECLARE_GETTER(
  ltypes,
  "List of logical types for all columns")

DECLARE_GETTER(
  stypes,
  "List of \"storage\" types for all columns")

DECLARE_GETTER(
  rowindex_type,
  "Type of the row index: 'slice' or 'array'")

DECLARE_GETTER(
  rowindex,
  "Row index of the view datatable, or None if this is not a view datatable")

DECLARE_GETTER(
  datatable_ptr,
  "Get pointer (converted to an int) to the wrapped DataTable object")

DECLARE_GETTER(
  alloc_size,
  "DataTable's internal size, in bytes")



//---- Methods -----------------------------------------------------------------

DECLARE_METHOD(
  window,
  "window(row0, row1, col0, col1)\n\n"
  "Retrieve datatable's data within a window")

DECLARE_METHOD(
  check,
  "check(stream=STDOUT)\n\n"
  "Check the DataTable for internal consistency. Returns False if any\n"
  "internal problem is found, or True otherwise.\n"
  "This method accepts a single optional parameter 'stream', which should be\n"
  "a file-like object where the error messages if any will be written. If \n"
  "this parameter is not given, then errors will be written to STDOUT.\n"
)

DECLARE_METHOD(
  column,
  "column(index)\n\n"
  "Get the requested column in the datatable")

DECLARE_METHOD(
  delete_columns,
  "Remove the specified list of columns from the datatable")

DECLARE_METHOD(
  rbind,
  "Append rows of other datatables to the current")

DECLARE_METHOD(
  cbind,
  "Append columns of other datatables to the current")

DECLARE_METHOD(
  sort,
  "Sort datatable according to a column")

DECLARE_METHOD(
  materialize,
  "materialize()\n\n"
  "Convert DataTable from 'view' into 'data' representation.\n")

DECLARE_METHOD(
  apply_na_mask,
  "apply_na_mask(mask)\n\n")

DECLARE_METHOD(
  use_stype_for_buffers,
  "use_stype_for_buffers(stype)\n\n")

DECLARE_METHOD(
   get_min,
   "Get the minimum for each column in the datatable")

DECLARE_METHOD(
  get_max,
  "Get the maximum for each column in the datatable")

DECLARE_METHOD(
  get_sum,
  "Get the sum for each column in the datatable")

DECLARE_METHOD(
  get_mean,
  "Get the mean for each column in the datatable")

DECLARE_METHOD(
  get_sd,
  "Get the standard deviation for each column in the datatable")

DECLARE_METHOD(
  get_countna,
  "Get the NA count for each column in the datatable")



//---- Python API --------------------------------------------------------------

DECLARE_FUNCTION(
  datatable_assemble,
  "datatable_assemble(columns)\n\n",
  HOMEFLAG)

DECLARE_FUNCTION(
  datatable_from_list,
  "datatable_from_list(...)\n\n"
  "Create a DataTable from ...",
  HOMEFLAG)

DECLARE_FUNCTION(
  datatable_load,
  "datatable_load(...)\n\n",
  HOMEFLAG)

DECLARE_FUNCTION(
  install_buffer_hooks,
  "install_buffer_hooks(...)\n\n",
  PY_BUFFERS_cc)

DECLARE_FUNCTION(
  datatable_from_buffers,
  "datatable_from_buffers(buffers: List)\n\n"
  "Load datatable from a list of Python objects supporting Buffers protocol.\n"
  "This is typically a list of numpy arrays, and this function is invoked\n"
  "when converting a pandas DataFrame into a DataTable (each column in pandas\n"
  "has its own type, and is backed by a separate numpy array).\n",
  PY_BUFFERS_cc)


};
#undef BASECLS
#undef HOMEFLAG
#endif
