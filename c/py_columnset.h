//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#ifndef dt_PY_COLUMNSET_h
#define dt_PY_COLUMNSET_h
#include <Python.h>
#include "datatable.h"
#include "rowindex.h"
#include "py_utils.h"


#define BASECLS pycolumnset::obj
#define CLSNAME ColumnSet
#define HOMEFLAG dt_PY_COLUMNSET_cc
namespace pycolumnset
{

struct obj : public PyObject {
  Column** columns;
  size_t ncols;
};

extern PyTypeObject type;


//---- Generic info ------------------------------------------------------------

DECLARE_INFO(
  datatable.core.ColumnSet,
  "Array of columns that can be used to construct a DataTable.")

DECLARE_REPR()
DECLARE_DESTRUCTOR()



//---- Methods -----------------------------------------------------------------

DECLARE_METHOD(
  to_frame,
  "to_frame(self, names)\n"
  "--\n\n"
  "Convert this ColumnSet into a Frame, with the provided names.")

DECLARE_METHOD(
  append_columns,
  "Add another ColumnSet to the current. This append uses move semantics:\n"
  "after this call, the other ColumnSet becomes empty.")



//---- External API ------------------------------------------------------------

DECLARE_FUNCTION(
  columns_from_slice,
  "columns_from_slice(dt, rowindex, start, count, step)\n\n"
  "Retrieve set of columns as a slice of columns in DataTable `dt`.\n",
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
#undef CLSNAME
#undef HOMEFLAG
#endif
