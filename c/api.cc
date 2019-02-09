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
#include "datatable.h"
#include "frame/py_frame.h"
#include "rowindex.h"
#include "py_rowindex.h"
extern "C" {


//------------------------------------------------------------------------------
// Helper functions
//------------------------------------------------------------------------------

static int _column_index_oob(DataTable* dt, size_t i) {
  if (i < dt->ncols) return 0;
  PyErr_Format(PyExc_IndexError, "Column %zu does not exist in the Frame", i);
  return -1;
}

static DataTable* _extract_dt(PyObject* pydt) {
  return static_cast<py::Frame*>(pydt)->get_datatable();
}

static RowIndex* _extract_ri(PyObject* pyri) {
  if (pyri == Py_None) return nullptr;
  return static_cast<py::orowindex::pyobject*>(pyri)->ri;
}


size_t DtABIVersion() {
  return 1;
}



//------------------------------------------------------------------------------
// Frame
//------------------------------------------------------------------------------

int DtFrame_Check(PyObject* ob) {
  if (ob == nullptr) return 0;
  auto typeptr = reinterpret_cast<PyObject*>(&py::Frame::Type::type);
  int ret = PyObject_IsInstance(ob, typeptr);
  if (ret == -1) {
    PyErr_Clear();
    ret = 0;
  }
  return ret;
}


size_t DtFrame_NColumns(PyObject* pydt) {
  auto dt = _extract_dt(pydt);
  return dt->ncols;
}

size_t DtFrame_NRows(PyObject* pydt) {
  auto dt = _extract_dt(pydt);
  return dt->nrows;
}


int DtFrame_ColumnStype(PyObject* pydt, size_t i) {
  auto dt = _extract_dt(pydt);
  if (_column_index_oob(dt, i)) return -1;
  return static_cast<int>(dt->columns[i]->stype());  // stype() is noexcept
}


PyObject* DtFrame_ColumnRowindex(PyObject* pydt, size_t i) {
  auto dt = _extract_dt(pydt);
  if (_column_index_oob(dt, i)) return nullptr;
  const RowIndex& ri = dt->columns[i]->rowindex();  // rowindex() is noexcept
  return (ri? py::orowindex(ri) : py::None()).release();
}


const void* DtFrame_ColumnDataR(PyObject* pydt, size_t i) {
  auto dt = _extract_dt(pydt);
  if (_column_index_oob(dt, i)) return nullptr;
  try {
    return dt->columns[i]->data();
  } catch (const std::exception& e) {
    exception_to_python(e);
    return nullptr;
  }
}

void* DtFrame_ColumnDataW(PyObject* pydt, size_t i) {
  auto dt = _extract_dt(pydt);
  if (_column_index_oob(dt, i)) return nullptr;
  try {
    return dt->columns[i]->data_w();
  } catch (const std::exception& e) {
    exception_to_python(e);
    return nullptr;
  }
}


const char* DtFrame_ColumnStringDataR(PyObject* pydt, size_t i) {
  auto dt = _extract_dt(pydt);
  if (_column_index_oob(dt, i)) return nullptr;
  SType st = dt->columns[i]->stype();
  try {
    if (st == SType::STR32) {
      auto scol = static_cast<StringColumn<uint32_t>*>(dt->columns[i]);
      return scol->strdata();
    }
    if (st == SType::STR64) {
      auto scol = static_cast<StringColumn<uint64_t>*>(dt->columns[i]);
      return scol->strdata();
    }
  } catch (const std::exception& e) {
    exception_to_python(e);
    return nullptr;
  }
  PyErr_Format(PyExc_TypeError, "Column %zu is not of string type", i);
  return nullptr;
}



//------------------------------------------------------------------------------
// Rowindex
//------------------------------------------------------------------------------

int DtRowindex_Check(PyObject* ob) {
  if (ob == Py_None) return 1;
  return py::orowindex::check(ob);
}


int DtRowindex_Type(PyObject* pyri) {
  RowIndex* ri = _extract_ri(pyri);
  if (!ri) return 0;
  return static_cast<int>(ri->type());
}


size_t DtRowindex_Size(PyObject* pyri) {
  RowIndex* ri = _extract_ri(pyri);
  if (!ri) return 0;
  return ri->size();
}


int DtRowindex_UnpackSlice(
    PyObject* pyri, size_t* start, size_t* length, size_t* step)
{
  RowIndex* ri = _extract_ri(pyri);
  if (!ri || ri->type() != RowIndexType::SLICE) {
    PyErr_Format(PyExc_TypeError, "expected a slice rowindex");
    return -1;
  }
  *start = ri->slice_start();
  *length = ri->size();
  *step = ri->slice_step();
  return 0;
}


const void* DtRowindex_ArrayData(PyObject* pyri) {
  RowIndex* ri = _extract_ri(pyri);
  if (ri && ri->type() == RowIndexType::ARR32) {
    return ri->indices32();
  }
  if (ri && ri->type() == RowIndexType::ARR64) {
    return ri->indices64();
  }
  PyErr_Format(PyExc_TypeError, "expected an array rowindex");
  return nullptr;
}



} // extern "C"
