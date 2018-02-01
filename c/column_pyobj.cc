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
  // If mbuf is either an instance of ExternalMemBuf (owned externally), or
  // shared with some other user (refcount > 1) then we don't want to decref
  // the objects in the buffer.
  ExternalMemBuf* xbuf = dynamic_cast<ExternalMemBuf*>(mbuf);
  if (mbuf->get_refcount() == 1 && !xbuf) {
    PyObject** elems = elements();
    // The number of elements in the buffer may be different from `nrows` if
    // there is a RowIndex applied.
    size_t nelems = mbuf->size() / sizeof(PyObject*);
    for (size_t i = 0; i < nelems; ++i) {
      Py_DECREF(elems[i]);
    }
  }
  mbuf->release();
  // make sure mbuf doesn't get released second time in upstream destructor
  mbuf = nullptr;
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


