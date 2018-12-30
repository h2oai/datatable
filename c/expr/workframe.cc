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
namespace dt {


workframe::workframe(DataTable* dt) {
  // The source frame must have flag `natural=false` so that `allcols_jn`
  // knows to select all columns from it.
  frames.push_back(subframe {dt, RowIndex(), false});
  mode = EvalMode::SELECT;
}


void workframe::set_mode(EvalMode m) {
  mode = m;
}


EvalMode workframe::get_mode() const {
  return mode;
}


void workframe::add_subframe(DataTable* dt) {
  frames.push_back(subframe {dt, RowIndex(), true});
}


DataTable* workframe::get_datatable(size_t i) const {
  return frames[i].dt;
}


const RowIndex& workframe::get_rowindex(size_t i) const {
  return frames[i].ri;
}


const Groupby& workframe::get_groupby() const {
  return gb;
}


bool workframe::is_naturally_joined(size_t i) const {
  return frames[i].natural;
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
