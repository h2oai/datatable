//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#define dt_EXPR_PY_EXPR_CC
#include "expr/py_expr.h"
#include <Python.h>
#include "python/obj.h"
#include "py_column.h"


PyObject* expr_binaryop(PyObject*, PyObject* args)
{
  int opcode;
  PyObject* arg1;
  PyObject* arg2;
  if (!PyArg_ParseTuple(args, "iOO:expr_binaryop", &opcode, &arg1, &arg2))
    return nullptr;
  py::bobj py_lhs(arg1);
  py::bobj py_rhs(arg2);

  Column* lhs = py_lhs.to_column();
  Column* rhs = py_rhs.to_column();
  Column* res = expr::binaryop(opcode, lhs, rhs);
  return pycolumn::from_column(res, nullptr, 0);
}


PyObject* expr_cast(PyObject*, PyObject* args)
{
  int stype;
  PyObject* arg1;
  if (!PyArg_ParseTuple(args, "Oi:expr_cast", &arg1, &stype))
    return nullptr;
  py::bobj pyarg(arg1);

  Column* col = pyarg.to_column();
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
  py::bobj pyarg1(arg1);
  py::bobj pyarg3(arg3);
  DataTable* dt = pyarg1.to_frame();
  RowIndex ri = pyarg3.to_rowindex();

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
  py::bobj pyarg1(arg1);
  py::bobj pyarg2(arg2);

  Column* col = pyarg1.to_column();
  Groupby* grpby = pyarg2.to_groupby();
  Column* res = nullptr;
  if (grpby) {
    res = expr::reduceop(opcode, col, *grpby);
  } else {
    Groupby gb = Groupby::single_group(col->nrows);
    res = expr::reduceop(opcode, col, gb);
  }
  return pycolumn::from_column(res, nullptr, 0);
}

PyObject* expr_count(PyObject*, PyObject* args)
{
  PyObject* arg1;
  PyObject* arg2;
  if (!PyArg_ParseTuple(args, "OO:expr_count", &arg1, &arg2))
    return nullptr;

  DataTable* dt = py::bobj(arg1).to_frame();
  Groupby* grpby = py::bobj(arg2).to_groupby();

  Column* res = nullptr;

  // If there is no Groupby object, return number of rows in dataframe
  if (grpby == nullptr) {
    res = Column::new_data_column(SType::INT64, static_cast<int64_t>(1));
    auto d_res = static_cast<int64_t*>(res->data_w());
    d_res[0] = dt->nrows;

  // If there is, return number of rows in each group
  } else {
    size_t ng = grpby->ngroups();
    const int32_t* offsets = grpby->offsets_r();

    res = Column::new_data_column(SType::INT32, static_cast<int32_t>(ng));
    auto d_res = static_cast<int32_t*>(res->data_w());

    for (size_t i = 0; i < ng; ++i) {
      d_res[i] = offsets[i + 1] - offsets[i];
    }
  }

  return pycolumn::from_column(res, nullptr, 0);
}


PyObject* expr_unaryop(PyObject*, PyObject* args)
{
  int opcode;
  PyObject* arg1;
  if (!PyArg_ParseTuple(args, "iO:expr_isna", &opcode, &arg1))
    return nullptr;
  py::bobj pyarg1(arg1);

  Column* col = pyarg1.to_column();
  Column* res = expr::unaryop(opcode, col);
  return pycolumn::from_column(res, nullptr, 0);
}


