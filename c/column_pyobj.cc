//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "column.h"
#include "utils.h"          // set_value, add_ptr
#include "utils/assert.h"



PyObjectColumn::PyObjectColumn() : FwColumn<PyObject*>() {}

PyObjectColumn::PyObjectColumn(int64_t nrows_, MemoryBuffer* mb) :
    FwColumn<PyObject*>(nrows_, mb) {}


PyObjectColumn::~PyObjectColumn() {
  if (!mbuf) return;
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


void PyObjectColumn::resize_and_fill(int64_t new_nrows)
{
  if (new_nrows == nrows) return;

  if (new_nrows < nrows) {
    for (int64_t i = new_nrows; i < nrows; ++i) {
      Py_DECREF(mbuf->get_elem<PyObject*>(i));
    }
  }

  mbuf = mbuf->safe_resize(sizeof(PyObject*) * static_cast<size_t>(new_nrows));

  if (new_nrows > nrows) {
    // Replicate the value or fill with NAs
    PyObject* fill_value = nrows == 1? get_elem(0) : na_elem;
    for (int64_t i = nrows; i < new_nrows; ++i) {
      mbuf->set_elem<PyObject*>(i, fill_value);
    }
    Py_REFCNT(fill_value) += new_nrows - nrows;
  }
  this->nrows = new_nrows;

  // TODO(#301): Temporary fix.
  if (this->stats != nullptr) this->stats->reset();
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


void PyObjectColumn::rbind_impl(
  std::vector<const Column*>& columns, int64_t nnrows, bool col_empty)
{
  PyObject* na = Py_None;
  const void* naptr = static_cast<const void*>(&na);

  // Reallocate the column's data buffer
  size_t old_nrows = static_cast<size_t>(nrows);
  size_t new_nrows = static_cast<size_t>(nnrows);
  size_t old_alloc_size = sizeof(PyObject*) * old_nrows;
  size_t new_alloc_size = sizeof(PyObject*) * new_nrows;
  mbuf = mbuf->safe_resize(new_alloc_size);
  xassert(!mbuf->is_readonly());
  nrows = nnrows;

  // Copy the data
  void* resptr = mbuf->at(col_empty ? 0 : old_alloc_size);
  size_t nrows_to_fill = col_empty ? old_nrows : 0;
  size_t nrows_filled = 0;
  for (const Column* col : columns) {
    if (col->stype() == ST_VOID) {
      nrows_to_fill += static_cast<size_t>(col->nrows);
    } else {
      if (nrows_to_fill) {
        set_value(resptr, naptr, sizeof(PyObject*), nrows_to_fill);
        resptr = add_ptr(resptr, nrows_to_fill * sizeof(PyObject*));
        nrows_filled += nrows_to_fill;
        nrows_to_fill = 0;
      }
      if (col->stype() != ST_OBJECT_PYPTR) {
        Column* newcol = col->cast(stype());
        delete col;
        col = newcol;
      }
      memcpy(resptr, col->data(), col->alloc_size());
      PyObject** out = static_cast<PyObject**>(resptr);
      for (int64_t i = 0; i < col->nrows; ++i) {
        Py_INCREF(out[i]);
      }
      resptr = out + col->nrows;
    }
    delete col;
  }
  if (nrows_to_fill) {
    set_value(resptr, naptr, sizeof(PyObject*), nrows_to_fill);
    resptr = add_ptr(resptr, nrows_to_fill * sizeof(PyObject*));
    nrows_filled += nrows_to_fill;
  }
  xassert(resptr == mbuf->at(new_alloc_size));
  na->ob_refcnt += nrows_filled;
}



//----- Type casts -------------------------------------------------------------

void PyObjectColumn::cast_into(PyObjectColumn* target) const {
  PyObject** src_data = this->elements();
  for (int64_t i = 0; i < nrows; ++i) {
    Py_INCREF(src_data[i]);
  }
  memcpy(target->data(), this->data(), alloc_size());
}



//----- Stats ------------------------------------------------------------------

PyObjectStats* PyObjectColumn::get_stats() const {
  if (stats == nullptr) stats = new PyObjectStats();
  return static_cast<PyObjectStats*>(stats);
}
