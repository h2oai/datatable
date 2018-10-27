//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "column.h"
#include "python/obj.h"
#include "python/string.h"
#include "utils/assert.h"



PyObjectColumn::PyObjectColumn() : FwColumn<PyObject*>() {
  mbuf.set_pyobjects(/*clear_data = */ true);
}

PyObjectColumn::PyObjectColumn(size_t nrows_) : FwColumn<PyObject*>(nrows_) {
  mbuf.set_pyobjects(/*clear_data = */ true);
}

PyObjectColumn::PyObjectColumn(size_t nrows_, MemoryRange&& mb)
    : FwColumn<PyObject*>(nrows_, std::move(mb))
{
  mbuf.set_pyobjects(/*clear_data = */ true);
}



SType PyObjectColumn::stype() const {
  return SType::OBJ;
}

py::oobj PyObjectColumn::get_value_at_index(int64_t i) const {
  int64_t j = (this->ri).nth(i);
  PyObject* x = this->elements_r()[j];
  return py::oobj(x);
}


// "PyObject" columns cannot be properly saved. So if somehow they were, then
// when opening, we'll just fill the column with NAs.
void PyObjectColumn::open_mmap(const std::string&, bool) {
  xassert(!ri);
  mbuf = MemoryRange::mem(static_cast<size_t>(nrows) * sizeof(PyObject*))
         .set_pyobjects(/*clear_data = */ true);
}


void PyObjectColumn::fill_na() {
  // This is called from `Column::new_na_column()` only; and for a
  // PyObjectColumn the buffer is already created containing Py_None values,
  // thus we don't need to do anything extra.
  // Semantics of this function may be clarified in the future (specifically,
  // is it ever called on a column with data?)
}


void PyObjectColumn::replace_buffer(MemoryRange&& new_mbuf) {
  xassert(new_mbuf.is_pyobjects());
  FwColumn<PyObject*>::replace_buffer(std::move(new_mbuf));
}


void PyObjectColumn::resize_and_fill(size_t new_nrows) {
  if (new_nrows == nrows) return;

  mbuf.resize(sizeof(PyObject*) * new_nrows);

  if (nrows == 1) {
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
  this->nrows = new_nrows;

  // TODO(#301): Temporary fix.
  if (this->stats != nullptr) this->stats->reset();
}


void PyObjectColumn::reify() {
  if (ri.isabsent()) return;
  FwColumn<PyObject*>::reify();

  // ???
  // After regular reification, we need to increment ref-counts for each
  // element in the column, since we created a new independent reference of
  // each python object.
  PyObject** data = this->elements_w();
  for (size_t i = 0; i < nrows; ++i) {
    Py_INCREF(data[i]);
  }
}


void PyObjectColumn::rbind_impl(
  std::vector<const Column*>& columns, size_t nnrows, bool col_empty)
{
  size_t old_nrows = static_cast<size_t>(nrows);
  size_t new_nrows = static_cast<size_t>(nnrows);

  // Reallocate the column's data buffer
  // `resize` fills all new elements with Py_None
  mbuf.resize(sizeof(PyObject*) * new_nrows);
  nrows = nnrows;

  // Copy the data
  PyObject** dest_data = static_cast<PyObject**>(mbuf.wptr());
  PyObject** dest_data0 = dest_data;
  if (!col_empty) {
    dest_data += old_nrows;
  }
  for (const Column* col : columns) {
    if (col->stype() == SType::VOID) {
      dest_data += static_cast<size_t>(col->nrows);
    } else {
      if (col->stype() != SType::OBJ) {
        Column* newcol = col->cast(stype());
        delete col;
        col = newcol;
      }
      auto src_data = static_cast<PyObject* const*>(col->data());
      for (size_t i = 0; i < col->nrows; ++i) {
        Py_INCREF(src_data[i]);
        Py_DECREF(*dest_data);
        *dest_data = src_data[i];
        dest_data++;
      }
    }
    delete col;
  }
  xassert(dest_data == dest_data0 + new_nrows);
  (void)dest_data0;
}



//----- Type casts -------------------------------------------------------------
using MWBPtr = std::unique_ptr<MemoryWritableBuffer>;

template<typename OT>
inline static MemoryRange cast_str_helper(
  size_t nrows, const PyObject* const* src, OT* toffsets)
{
  // Warning: Do not attempt to parallelize this: creating new PyObjects
  // is not thread-safe! In addition `to_pystring_force()` may invoke
  // arbitrary python code when stringifying a value...
  size_t exp_size = static_cast<size_t>(nrows) * sizeof(PyObject*);
  auto wb = MWBPtr(new MemoryWritableBuffer(exp_size));
  OT offset = 0;
  toffsets[-1] = 0;
  for (size_t i = 0; i < nrows; ++i) {
    py::ostring xstr = py::obj(src[i]).to_pystring_force();
    CString xcstr = xstr.to_cstring();
    if (xcstr.ch) {
      wb->write(static_cast<size_t>(xcstr.size), xcstr.ch);
      offset += static_cast<OT>(xcstr.size);
      toffsets[i] = offset;
    } else {
      toffsets[i] = offset | GETNA<OT>();
    }
  }
  wb->finalize();
  return wb->get_mbuf();
}


void PyObjectColumn::cast_into(PyObjectColumn* target) const {
  PyObject* const* src_data = this->elements_r();
  PyObject** dest_data = target->elements_w();
  for (size_t i = 0; i < nrows; ++i) {
    Py_INCREF(src_data[i]);
    Py_DECREF(dest_data[i]);
    dest_data[i] = src_data[i];
  }
}


void PyObjectColumn::cast_into(StringColumn<uint32_t>* target) const {
  const PyObject* const* data = elements_r();
  uint32_t* offsets = target->offsets_w();
  MemoryRange strbuf = cast_str_helper<uint32_t>(nrows, data, offsets);
  target->replace_buffer(target->data_buf(), std::move(strbuf));
}


void PyObjectColumn::cast_into(StringColumn<uint64_t>* target) const {
  const PyObject* const* data = elements_r();
  uint64_t* offsets = target->offsets_w();
  MemoryRange strbuf = cast_str_helper<uint64_t>(nrows, data, offsets);
  target->replace_buffer(target->data_buf(), std::move(strbuf));
}



//----- Stats ------------------------------------------------------------------

PyObjectStats* PyObjectColumn::get_stats() const {
  if (stats == nullptr) stats = new PyObjectStats();
  return static_cast<PyObjectStats*>(stats);
}
