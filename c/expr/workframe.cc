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
#include "expr/workframe.h"
#include "frame/py_frame.h"
namespace dt {


GroupbyMode common_mode(GroupbyMode x, GroupbyMode y) {
  return static_cast<uint8_t>(x) > static_cast<uint8_t>(y)? x : y;
}



//------------------------------------------------------------------------------
// workframe
//------------------------------------------------------------------------------

workframe::workframe(DataTable* dt) {
  // The source frame must have flag `natural=false` so that `allcols_jn`
  // knows to select all columns from it.
  frames.push_back(subframe {dt, RowIndex(), false});
  mode = EvalMode::SELECT;
  groupby_mode = GroupbyMode::NONE;
  result = nullptr;
}


void workframe::set_mode(EvalMode m) {
  mode = m;
}


EvalMode workframe::get_mode() const {
  return mode;
}


void workframe::add_join(py::ojoin oj) {
  DataTable* dt = oj.get_datatable();
  frames.push_back(subframe {dt, RowIndex(), true});
}


void workframe::add_groupby(py::oby og) {
  by_node = og.to_by_node(*this);
}


void workframe::add_i(py::oobj oi) {
  iexpr = i_node::make(oi, *this);
}


void workframe::add_j(py::oobj oj) {
  jexpr = j_node::make(oj, *this);
}


void workframe::evaluate() {
  // Compute joins
  if (frames.size() > 1) {
    DataTable* xdt = frames[0].dt;
    for (size_t i = 1; i < frames.size(); ++i) {
      DataTable* jdt = frames[i].dt;
      frames[i].ri = natural_join(xdt, jdt);
    }
  }
  // Compute groupby
  if (by_node) {
    by_node->execute(*this);
  }
  // Compute i expression
  iexpr->execute(*this);

  switch (mode) {
    case EvalMode::SELECT: result = jexpr->select(*this); break;
    case EvalMode::DELETE: jexpr->delete_(*this); break;
    case EvalMode::UPDATE: jexpr->update(*this); break;
  }
}


py::oobj workframe::get_result() const {
  return result? py::oobj::from_new_reference(py::Frame::from_datatable(result))
               : py::None();
}


DataTable* workframe::get_datatable(size_t i) const {
  return frames[i].dt;
}


const RowIndex& workframe::get_rowindex(size_t i) const {
  return frames[i].ri;
}


const Groupby& workframe::get_groupby() const {
  xassert(by_node);
  return by_node->gb;
}


const by_node_ptr& workframe::get_by_node() const {
  return by_node;
}


bool workframe::is_naturally_joined(size_t i) const {
  return frames[i].natural;
}

bool workframe::has_groupby() const {
  return bool(by_node);
}


size_t workframe::nframes() const {
  return frames.size();
}


size_t workframe::nrows() const {
  const RowIndex& ri0 = frames[0].ri;
  return ri0? ri0.size() : frames[0].dt->nrows;
}


void workframe::apply_rowindex(const RowIndex& ri) {
  for (size_t i = 0; i < frames.size(); ++i) {
    frames[i].ri = ri * frames[i].ri;
  }
}



} // namespace dt
