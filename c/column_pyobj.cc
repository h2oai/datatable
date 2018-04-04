//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "column.h"
#include "utils/assert.h"



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


// "PyObj" columns cannot be properly saved. So if somehow they were, then when
// opening, we'll just fill the column with NAs.
void PyObjectColumn::open_mmap(const std::string&) {
  xassert(!ri && !mbuf);
  mbuf = new MemoryMemBuf(static_cast<size_t>(nrows) * sizeof(PyObject*));
  fill_na();
}


void PyObjectColumn::fill_na() {
  PyObject** data = this->elements();
  for (int64_t i = 0; i < nrows; ++i) {
    data[i] = Py_None;
  }
  Py_None->ob_refcnt += nrows;  // Manually increase ref-count on Py_None
}


void PyObjectColumn::reify() {
  if (ri.isabsent()) return;
  FwColumn<PyObject*>::reify();

  // After regular reification, we need to increment ref-counts for each
  // element in the column, since we created a new independent reference of
  // each python object.
  PyObject** data = this->elements();
  for (int64_t i = 0; i < nrows; ++i) {
    Py_INCREF(data[i]);
  }
}


//----- Type casts -------------------------------------------------------------

void PyObjectColumn::cast_into(PyObjectColumn* target) const {
  PyObject** src_data = this->elements();
  for (int64_t i = 0; i < this->nrows; ++i) {
    Py_XINCREF(src_data[i]);
  }
  memcpy(target->data(), this->data(), alloc_size());
}



//----- Stats ------------------------------------------------------------------

PyObjectStats* PyObjectColumn::get_stats() const {
  if (stats == nullptr) stats = new PyObjectStats();
  return static_cast<PyObjectStats*>(stats);
}
