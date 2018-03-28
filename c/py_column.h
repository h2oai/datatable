//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#ifndef dt_PY_COLUMN_h
#define dt_PY_COLUMN_h
#include <Python.h>
#include "column.h"
#include "py_datatable.h"
#include "py_utils.h"


#define BASECLS pycolumn::obj
#define CLSNAME Column
#define HOMEFLAG dt_PY_COLUMN_cc
namespace pycolumn
{

struct obj : public PyObject {
  // PyObject_HEAD
  Column* ref;
  pydatatable::obj* pydt;
  int64_t colidx;
};


extern PyTypeObject type;
extern PyBufferProcs as_buffer;
extern PyObject* fn_hexview;


pycolumn::obj* from_column(Column*, pydatatable::obj*, int64_t);
int unwrap(PyObject* source, Column** target);
int static_init(PyObject* module);



//---- Generic info ------------------------------------------------------------

DECLARE_INFO(
  datatable.core.Column,
  "Single Column within a DataTable.")

DECLARE_DESTRUCTOR()



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
  "Pointer (cast to int64_t) to the column's internal memory buffer.\n"
  "This pointer may only be used immediately upon acquiral. The pointer may \n"
  "become invalid if the column is modified or garbage-collected, and also \n"
  "when .data_pointer of some other column is accessed. Reading from an \n"
  "invalid pointer may return incorrect data, or result in a seg.fault.")

DECLARE_GETTER(
  refcount,
  "Reference count of the column")

DECLARE_GETTER(
  meta,
  "String representation of the column's `meta` struct")

DECLARE_GETTER(
  nrows,
  "Return the number of rows in this column")


//---- Methods -----------------------------------------------------------------

DECLARE_METHOD(
  save_to_disk,
  "save_to_disk(filename, _strategy)\n\n"
  "Save this column's data into the file `filename`, using the provided\n"
  "writing strategy.\n")

DECLARE_METHOD(
  hexview,
  "hexview()\n\n"
  "View column's internal data as hex bytes.\n")


//---- Python API --------------------------------------------------------------

DECLARE_FUNCTION(
  column_from_list,
  "column_from_list(list)\n\n"
  "Convert a Python list into a Column object.",
  HOMEFLAG)


};  // namespace pycolumn
#undef BASECLS
#undef CLSNAME
#undef HOMEFLAG
#endif
