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
#include "expr/collist.h"
#include "expr/workframe.h"
#include "frame/py_frame.h"
namespace dt {


//------------------------------------------------------------------------------
// workframe
//------------------------------------------------------------------------------

workframe::workframe(DataTable* dt) {
  // The source frame must have flag `natural=false` so that `allcols_jn`
  // knows to select all columns from it.
  frames.push_back(subframe {dt, RowIndex(), false});
  mode = EvalMode::SELECT;
  groupby_mode = GroupbyMode::NONE;
}


void workframe::set_mode(EvalMode m) {
  mode = m;
}


EvalMode workframe::get_mode() const {
  return mode;
}


GroupbyMode workframe::get_groupby_mode() const {
  return groupby_mode;
}


void workframe::add_join(py::ojoin oj) {
  DataTable* dt = oj.get_datatable();
  frames.push_back(subframe {dt, RowIndex(), true});
}


void workframe::add_groupby(py::oby og) {
  byexpr.add_groupby_columns(og.cols(*this));
}


void workframe::add_sortby(py::osort obj) {
  byexpr.add_sortby_columns(obj.cols(*this));
}


void workframe::add_i(py::oobj oi) {
  xassert(!iexpr);
  iexpr = i_node::make(oi, *this);
}


void workframe::add_j(py::oobj oj) {
  xassert(!jexpr);
  jexpr = j_node::make(oj, *this);
}


void workframe::add_replace(py::oobj obj) {
  xassert(!repl);
  repl = repl_node::make(*this, obj);
}


void workframe::evaluate() {
  // Compute joins
  DataTable* xdt = frames[0].dt;
  for (size_t i = 1; i < frames.size(); ++i) {
    DataTable* jdt = frames[i].dt;
    frames[i].ri = natural_join(xdt, jdt);
  }

  // Compute groupby
  if (byexpr) {
    groupby_mode = jexpr->get_groupby_mode(*this);
  }
  byexpr.execute(*this);

  // Compute i filter
  iexpr->execute(*this);

  switch (mode) {
    case EvalMode::SELECT:
      if (byexpr) {
        byexpr.create_columns(*this);
      }
      jexpr->select(*this);
      fix_columns();
      break;

    case EvalMode::DELETE:
      jexpr->delete_(*this);
      break;

    case EvalMode::UPDATE:
      jexpr->update(*this, repl.get());
      break;
  }
}


// After evaluation of the j node, the columns in `columns` may have different
// sizes: some are aggregated to group level, others have same number of rows
// as dt0. If this happens, we need to expand the "short" columns to the full
// size.
void workframe::fix_columns() {
  if (groupby_mode != GroupbyMode::GtoALL) return;
  xassert(byexpr);
  size_t nrows0 = get_datatable(0)->nrows;
  size_t ngrps = gb.ngroups();
  RowIndex ungroup_ri;

  for (size_t i = 0; i < columns.size(); ++i) {
    if (columns[i]->nrows == nrows0) continue;
    xassert(columns[i]->nrows == ngrps);
    if (!ungroup_ri) {
      ungroup_ri = gb.ungroup_rowindex();
    }
    const RowIndex& col_rowindex = columns[i]->rowindex();
    columns[i]->replace_rowindex(_product(ungroup_ri, col_rowindex));
  }
}


py::oobj workframe::get_result() {
  if (mode == EvalMode::SELECT) {
    DataTable* result = new DataTable(std::move(columns), std::move(colnames));
    if (result->ncols == 0) {
      // When selecting a 0-column subset, make sure the number of rows is the
      // same as if some of the columns were selected.
      result->nrows = frames[0].ri? frames[0].ri.size()
                                  : frames[0].dt->nrows;
    }
    return py::oobj::from_new_reference(py::Frame::from_datatable(result));
  }
  return py::None();
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


const by_node& workframe::get_by_node() const {
  return byexpr;
}


bool workframe::is_naturally_joined(size_t i) const {
  return frames[i].natural;
}

bool workframe::has_groupby() const {
  return bool(byexpr);
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




//---- Construct the resulting frame -------------

size_t workframe::size() const noexcept {
  return columns.size();
}


void workframe::reserve(size_t n) {
  size_t nn = n + columns.size();
  columns.reserve(nn);
  colnames.reserve(nn);
}


void workframe::add_column(
  const Column* col, const RowIndex& ri, std::string&& name)
{
  const RowIndex& ricol = col->rowindex();
  Column* newcol = col->shallowcopy(_product(ri, ricol));
  columns.push_back(newcol);
  colnames.push_back(std::move(name));
}


RowIndex& workframe::_product(const RowIndex& ra, const RowIndex& rb) {
  for (auto it = all_ri.rbegin(); it != all_ri.rend(); ++it) {
    if (it->first == ra) {
      return it->second;
    }
  }
  all_ri.push_back(std::make_pair(ra, ra * rb));
  return all_ri.back().second;
}



} // namespace dt
