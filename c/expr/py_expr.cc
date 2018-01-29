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
    return NULL;
  PyObj py_lhs(arg1);
  PyObj py_rhs(arg2);

  Column* lhs = py_lhs.as_column();
  Column* rhs = py_rhs.as_column();
  Column* res = expr::binaryop(opcode, lhs, rhs);
  return pycolumn::from_column(res, NULL, 0);
}


PyObject* expr_column(PyObject*, PyObject* args)
{
  int64_t index;
  PyObject* arg1;
  if (!PyArg_ParseTuple(args, "Ol:expr_column", &arg1, &index))
    return NULL;
  PyObj pyarg1(arg1);
  DataTable* dt = pyarg1.as_datatable();

  if (index < 0 || index >= dt->ncols) {
    PyErr_Format(PyExc_ValueError, "Invalid column index %lld", index);
  }
  Column* col = dt->columns[index]->shallowcopy();
  col->reify();
  return pycolumn::from_column(col, NULL, 0);
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
