//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#define dt_EXPR_PY_EXPR_CC
#include <Python.h>
#include "expr/py_expr.h"
#include "utils/pyobj.h"
#include "py_column.h"


PyObject* expr_binaryop(PyObject*, PyObject* args)
{
  int opcode;
  PyObject* arg1;
  PyObject* arg2;
  if (!PyArg_ParseTuple(args, "iOO:expr_binaryop", &opcode, &arg1, &arg2))
    return nullptr;
  PyObj py_lhs(arg1);
  PyObj py_rhs(arg2);

  Column* lhs = py_lhs.as_column();
  Column* rhs = py_rhs.as_column();
  Column* res = expr::binaryop(opcode, lhs, rhs);
  return pycolumn::from_column(res, nullptr, 0);
}


PyObject* expr_cast(PyObject*, PyObject* args)
{
  int stype;
  PyObject* arg1;
  if (!PyArg_ParseTuple(args, "Oi:expr_cast", &arg1, &stype))
    return nullptr;
  PyObj pyarg(arg1);

  Column* col = pyarg.as_column();
  col->reify();
  Column* res = col->cast(static_cast<SType>(stype));
  return pycolumn::from_column(res, nullptr, 0);
}


PyObject* expr_column(PyObject*, PyObject* args)
{
  int64_t index;
  PyObject* arg1, *arg3;
  if (!PyArg_ParseTuple(args, "OlO:expr_column", &arg1, &index, &arg3))
    return nullptr;
  PyObj pyarg1(arg1);
  PyObj pyarg3(arg3);
  DataTable* dt = pyarg1.as_datatable();
  RowIndex ri = pyarg3.as_rowindex();

  if (index < 0 || index >= dt->ncols) {
    PyErr_Format(PyExc_ValueError, "Invalid column index %lld", index);
  }
  Column* col = dt->columns[index]->shallowcopy(ri);
  // col->reify();
  return pycolumn::from_column(col, nullptr, 0);
}


PyObject* expr_reduceop(PyObject*, PyObject* args)
{
  int opcode;
  PyObject* arg1, *arg2;
  if (!PyArg_ParseTuple(args, "iOO:expr_reduceop", &opcode, &arg1, &arg2))
    return nullptr;
  PyObj pyarg1(arg1);
  PyObj pyarg2(arg2);

  Column* col = pyarg1.as_column();
  Groupby* grpby = pyarg2.as_groupby();
  Column* res = nullptr;
  if (grpby) {
    res = expr::reduceop(opcode, col, *grpby);
  } else {
    Groupby gb = Groupby::single_group(col->nrows);
    res = expr::reduceop(opcode, col, gb);
  }
  return pycolumn::from_column(res, nullptr, 0);
}


PyObject* expr_unaryop(PyObject*, PyObject* args)
{
  int opcode;
  PyObject* arg1;
  if (!PyArg_ParseTuple(args, "iO:expr_isna", &opcode, &arg1))
    return nullptr;
  PyObj pyarg1(arg1);

  Column* col = pyarg1.as_column();
  Column* res = expr::unaryop(opcode, col);
  return pycolumn::from_column(res, nullptr, 0);
}


