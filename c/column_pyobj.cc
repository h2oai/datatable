//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "python/obj.h"
#include "python/string.h"
#include "utils/assert.h"
#include "column_impl.h"



PyObjectColumn::PyObjectColumn() : FwColumn<PyObject*>() {
  _stype = SType::OBJ;
  mbuf.set_pyobjects(/*clear_data = */ true);
}

PyObjectColumn::PyObjectColumn(size_t nrows_) : FwColumn<PyObject*>(nrows_) {
  _stype = SType::OBJ;
  mbuf.set_pyobjects(/*clear_data = */ true);
}

PyObjectColumn::PyObjectColumn(size_t nrows_, MemoryRange&& mb)
    : FwColumn<PyObject*>(nrows_, std::move(mb))
{
  _stype = SType::OBJ;
  mbuf.set_pyobjects(/*clear_data = */ true);
}



bool PyObjectColumn::get_element(size_t i, py::robj* out) const {
  size_t j = (this->ri)[i];
  if (j == RowIndex::NA) return true;
  PyObject* x = this->elements_r()[j];
  *out = x;
  return (x == Py_None);
}



void PyObjectColumn::fill_na() {
  // This is called from `Column::new_na_column()` only; and for a
  // PyObjectColumn the buffer is already created containing Py_None values,
  // thus we don't need to do anything extra.
  // Semantics of this function may be clarified in the future (specifically,
  // is it ever called on a column with data?)
}


void PyObjectColumn::resize_and_fill(size_t new_nrows) {
  if (new_nrows == _nrows) return;
  materialize();

  mbuf.resize(sizeof(PyObject*) * new_nrows);

  if (_nrows == 1) {
    // Replicate the value; the case when we need to fill with NAs is already
    // handled by `mbuf.resize()`
    PyObject* fill_value = get_elem(0);
    PyObject** dest_data = this->elements_w();
    for (size_t i = 1; i < new_nrows; ++i) {
      Py_DECREF(dest_data[i]);
      dest_data[i] = fill_value;
    }
    fill_value->ob_refcnt += new_nrows - 1;
  }
  this->_nrows = new_nrows;

  // TODO(#301): Temporary fix.
  if (this->stats != nullptr) this->stats->reset();
}


ColumnImpl* PyObjectColumn::materialize() {
  if (!ri) return this;

  MemoryRange newmr = MemoryRange::mem(sizeof(PyObject*) * _nrows);
  newmr.set_pyobjects(/* clear_data = */ false);

  auto data_dest = static_cast<PyObject**>(newmr.xptr());
  auto data_src = elements_r();
  ri.iterate(0, _nrows, 1,
    [&](size_t i, size_t j) {
      data_dest[i] = (j == RowIndex::NA)? Py_None : data_src[j];
      Py_INCREF(data_dest[i]);
    });

  mbuf = std::move(newmr);
  ri.clear();
  return this;
}
