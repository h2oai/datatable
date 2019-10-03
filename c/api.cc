//------------------------------------------------------------------------------
// Copyright 2018 H2O.ai
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
//------------------------------------------------------------------------------
#include "../datatable/include/datatable.h"
#include "frame/py_frame.h"
#include "datatable.h"
#include "rowindex.h"
extern "C" {


//------------------------------------------------------------------------------
// Helper functions
//------------------------------------------------------------------------------

static int _column_index_oob(DataTable* dt, size_t i) {
  if (i < dt->ncols()) return 0;
  PyErr_Format(PyExc_IndexError, "Column %zu does not exist in the Frame", i);
  return -1;
}

static DataTable* _extract_dt(PyObject* pydt) {
  return static_cast<py::Frame*>(pydt)->get_datatable();
}

size_t DtABIVersion() {
  return 2;
}



//------------------------------------------------------------------------------
// Frame
//------------------------------------------------------------------------------

int DtFrame_Check(PyObject* ob) {
  if (ob == nullptr) return 0;
  auto typeptr = reinterpret_cast<PyObject*>(&py::Frame::type);
  int ret = PyObject_IsInstance(ob, typeptr);
  if (ret == -1) {
    PyErr_Clear();
    ret = 0;
  }
  return ret;
}


size_t DtFrame_NColumns(PyObject* pydt) {
  auto dt = _extract_dt(pydt);
  return dt->ncols();
}

size_t DtFrame_NRows(PyObject* pydt) {
  auto dt = _extract_dt(pydt);
  return dt->nrows();
}


int DtFrame_ColumnStype(PyObject* pydt, size_t i) {
  auto dt = _extract_dt(pydt);
  if (_column_index_oob(dt, i)) return -1;
  return static_cast<int>(dt->get_column(i).stype());  // stype() is noexcept
}


int DtFrame_ColumnIsVirtual(PyObject* pydt, size_t i) {
  auto dt = _extract_dt(pydt);
  if (_column_index_oob(dt, i)) return -1;
  return dt->get_column(i).is_virtual();  // is_virtual() is noexcept
}


const void* DtFrame_ColumnDataR(PyObject* pydt, size_t i) {
  auto dt = _extract_dt(pydt);
  if (_column_index_oob(dt, i)) return nullptr;
  try {
    return dt->get_column(i).get_data_readonly();
  } catch (const std::exception& e) {
    exception_to_python(e);
    return nullptr;
  }
}

void* DtFrame_ColumnDataW(PyObject* pydt, size_t i) {
  auto dt = _extract_dt(pydt);
  if (_column_index_oob(dt, i)) return nullptr;
  try {
    return dt->get_column(i).get_data_editable();
  } catch (const std::exception& e) {
    exception_to_python(e);
    return nullptr;
  }
}


const char* DtFrame_ColumnStringDataR(PyObject* pydt, size_t i) {
  auto dt = _extract_dt(pydt);
  if (_column_index_oob(dt, i)) return nullptr;
  try {
    const Column& col = dt->get_column(i);
    if (col.ltype() == LType::STRING) {
      return static_cast<const char*>(col.get_data_readonly(1));
    }
  } catch (const std::exception& e) {
    exception_to_python(e);
    return nullptr;
  }
  PyErr_Format(PyExc_TypeError, "Column %zu is not of string type", i);
  return nullptr;
}



} // extern "C"
