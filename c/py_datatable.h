//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#ifndef dt_PY_DATATABLE_h
#define dt_PY_DATATABLE_h
#include <Python.h>
#include "datatable.h"
#include "options.h"
#include "py_utils.h"
namespace py { class Frame; }

#define BASECLS pydatatable::obj
#define CLSNAME DataTable
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
  py::Frame* _frame;
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

DECLARE_DESTRUCTOR()



//---- Getters/setters ---------------------------------------------------------

DECLARE_GETTER(
  isview,
  "Is the datatable view or now?")

DECLARE_GETTER(
  rowindex_type,
  "Type of the row index: 'slice' or 'array'")

DECLARE_GETTER(
  rowindex,
  "Row index of the view Frame, or None if this is not a view Frame")

DECLARE_GETSET(
  groupby,
  "Groupby applied to the Frame, or None if no groupby was applied")

DECLARE_GETSET(
  nkeys,
  "Number of key columns in the Frame")

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
  "Retrieve DataTable's data within a window")

DECLARE_METHOD(
  to_scalar,
  "to_scalar()\n\n"
  "For a 1x1 DataTable return its data as a Python scalar value. This in \n"
  "contrast with `window(...)` method which returns a list-of-lists \n"
  "regardless of the shape of the underlying DataTable.")

DECLARE_METHOD(
  check,
  "check()\n\n"
  "Check the DataTable for internal consistency. Raises an AssertionError if\n"
  "any internal problem is found.\n"
)

DECLARE_METHOD(
  column,
  "column(index)\n\n"
  "Get the requested column in the datatable")

DECLARE_METHOD(
  delete_columns,
  "Remove the specified list of columns from the datatable")

DECLARE_METHOD(
  replace_rowindex,
  "replace_rowindex(rowindex)\n\n"
  "Replace the current rowindex on the datatable with the new one.")

DECLARE_METHOD(
  replace_column_slice,
  "replace_column_slice(start, count, step, repl_at, replacement)\n\n"
  "Replace a slice of columns in the current DataTable with the columns\n"
  "from the provided DataTable `replacement` (which must be conformant).\n"
  "The values will be written at locations given by the RowIndex\n"
  "`repl_at`. This RowIndex may be missing, indicating that all values\n"
  "have to be replaced.\n")

DECLARE_METHOD(
  replace_column_array,
  "replace_column_array(arr, repl_at, replacement)\n\n"
  "Replace a selection of columns in the current DataTable with the columns\n"
  "from the `replacement` DataTable. The array `arr` contains the list of\n"
  "column indices to be replaced. It may also contain indices that are out\n"
  "of bounds for the current DataTable -- those indicate columns that should\n"
  "be appended rather than replaced.\n"
  "The RowIndex `repl_at`, if present, specifies rows at which the values\n"
  "have to be replaced. If `repl_at` is given, then `arr` cannot contain\n"
  "out-of-bounds column indices.\n")

DECLARE_METHOD(
  rbind,
  "Append rows of other datatables to the current")

DECLARE_METHOD(
  sort,
  "sort(col, makegroups=False)\n\n"
  "Sort datatable by the specified column and return the RowIndex object\n"
  "corresponding to the col's ordering. If `makegroups` is True, then\n"
  "grouping information will also be computed and stored in the RowIndex.")

DECLARE_METHOD(
  materialize,
  "materialize()\n\n"
  "Convert DataTable from 'view' into 'data' representation.\n")

DECLARE_METHOD(
  use_stype_for_buffers,
  "use_stype_for_buffers(stype)\n\n")

DECLARE_METHOD(
  save_jay,
  "save_jay(file, colnames)\n\n"
  "Save DataTable into a .jay file.\n")

DECLARE_METHOD(
  join,
  "join(rowindex, join_frame, cols)\n\n")


DECLARE_METHOD(
   get_min,
   "Get the minimum for each column in the DataTable")

DECLARE_METHOD(
  get_max,
  "Get the maximum for each column in the DataTable")

DECLARE_METHOD(
  get_sum,
  "Get the sum for each column in the DataTable")

DECLARE_METHOD(
  get_mean,
  "Get the mean for each column in the DataTable")

DECLARE_METHOD(
  get_sd,
  "Get the standard deviation for each column in the DataTable")

DECLARE_METHOD(
  get_skew,
  "Get the skew for each column in the DataTable")

DECLARE_METHOD(
  get_kurt,
  "Get the kurtosis for each column in the DataTable")

DECLARE_METHOD(
  get_countna,
  "Get the NA count for each column in the DataTable")

DECLARE_METHOD(
  get_nunique,
  "Get the number of unique values for each column in the DataTable")

DECLARE_METHOD(
  get_mode,
  "Get the most frequent value in each column in the DataTable")

DECLARE_METHOD(
  get_nmodal,
  "Get the count of most frequent values for each column in the DataTable")

DECLARE_METHOD(
   min1,
   "Get the scalar minimum of a single-column DataTable")

DECLARE_METHOD(
   max1,
   "Get the scalar maximum of a single-column DataTable")

DECLARE_METHOD(
   mode1,
   "Get the scalar mode of a single-column DataTable")

DECLARE_METHOD(
   sum1,
   "Get the scalar sum of a single-column DataTable")

DECLARE_METHOD(
   mean1,
   "Get the scalar mean of a single-column DataTable")

DECLARE_METHOD(
   sd1,
   "Get the scalar standard deviation of a single-column DataTable")

DECLARE_METHOD(
   countna1,
   "Get the scalar count of NAs in a single-column DataTable")

DECLARE_METHOD(
   nunique1,
   "Get the number of unique values in a single-column DataTable")

DECLARE_METHOD(
   nmodal1,
   "Get the number of modal values in a single-column DataTable")



//---- Python API --------------------------------------------------------------

DECLARE_FUNCTION(
  datatable_load,
  "datatable_load(...)\n\n",
  HOMEFLAG)

DECLARE_FUNCTION(
  open_jay,
  "open_jay(file)\n--\n\n"
  "Open a Frame from the provided .jay file.\n",
  HOMEFLAG)


DECLARE_FUNCTION(
  install_buffer_hooks,
  "install_buffer_hooks(...)\n\n",
  PY_BUFFERS_cc)


};
#undef BASECLS
#undef CLSNAME
#undef HOMEFLAG
#endif
