//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "column.h"

PyObjectColumn::PyObjectColumn() : FwColumn<PyObject*>() {}

PyObjectColumn::PyObjectColumn(int64_t nrows_, MemoryBuffer* mb) :
    FwColumn<PyObject*>(nrows_, mb) {}


PyObjectColumn::~PyObjectColumn() {
  // PyObject** elems = elements();
  // for (int64_t i = 0; i < nrows; i++) {
  //   Py_DECREF(elems[i]);
  // }
  // delete mbuf;
}

SType PyObjectColumn::stype() const {
  return ST_OBJECT_PYPTR;
}



//----- Type casts -------------------------------------------------------------

void PyObjectColumn::cast_into(PyObjectColumn* target) const {
  PyObject** src_data = this->elements();
  for (int64_t i = 0; i < this->nrows; ++i) {
    Py_XINCREF(src_data[i]);
  }
  memcpy(target->data(), this->data(), alloc_size());
}


