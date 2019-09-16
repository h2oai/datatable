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
#include "expr/expr.h"
#include "expr/workframe.h"
#include "expr/eval_context.h"
#include "frame/py_frame.h"
#include "column_impl.h"  // TODO: remove
namespace dt {


//------------------------------------------------------------------------------
// EvalContext
//------------------------------------------------------------------------------

EvalContext::EvalContext(DataTable* dt) {
  // The source frame must have flag `natural=false` so that `allcols_jn`
  // knows to select all columns from it.
  frames.push_back(subframe {dt, RowIndex(), false});
  mode = EvalMode::SELECT;
  groupby_mode = GroupbyMode::NONE;
}


void EvalContext::set_mode(EvalMode m) {
  mode = m;
}


EvalMode EvalContext::get_mode() const {
  return mode;
}


GroupbyMode EvalContext::get_groupby_mode() const {
  return groupby_mode;
}


void EvalContext::add_join(py::ojoin oj) {
  DataTable* dt = oj.get_datatable();
  frames.push_back(subframe {dt, RowIndex(), true});
}


void EvalContext::add_groupby(py::oby og) {
  byexpr.add_groupby_columns(*this, og.cols(*this));
}


void EvalContext::add_sortby(py::osort obj) {
  byexpr.add_sortby_columns(*this, obj.cols(*this));
}


void EvalContext::add_i(py::oobj oi) {
  xassert(!iexpr);
  iexpr = i_node::make(oi, *this);
}


void EvalContext::add_j(py::oobj oj) {
  xassert(!jexpr);
  jexpr2 = dt::expr::Expr(oj);
  jexpr = j_node::make(oj, *this);
}


void EvalContext::add_replace(py::oobj obj) {
  xassert(!repl);
  repl = repl_node::make(*this, obj);
}



//------------------------------------------------------------------------------
// Main evaluation
//------------------------------------------------------------------------------

void EvalContext::evaluate() {
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
  if (has_groupby()) {
    iexpr->execute_grouped(*this);
  } else {
    iexpr->execute(*this);
  }

  switch (mode) {
    case EvalMode::SELECT:
      if (byexpr) {
        byexpr.create_columns(*this);
        jexpr->select(*this);
        fix_columns();
      }
      else {
        auto res = jexpr2.evaluate_j(*this);
        out_datatable = std::move(res).convert_to_datatable();
      }
      break;

    case EvalMode::DELETE: evaluate_delete(); break;

    case EvalMode::UPDATE:
      jexpr->update(*this, repl.get());
      break;
  }
}




//------------------------------------------------------------------------------
// DELETE
//------------------------------------------------------------------------------

void EvalContext::evaluate_delete() {
  if (jexpr2.get_expr_kind() == expr::Kind::SliceAll) {
    evaluate_delete_rows();
  } else if (frames[0].ri) {
    evaluate_delete_subframe();
  } else {
    evaluate_delete_columns();
  }
}


void EvalContext::evaluate_delete_rows() {
  DataTable* dt0 = frames[0].dt;
  const RowIndex& ri0 = frames[0].ri;
  if (ri0) {
    RowIndex ri_neg = ri0.negate(dt0->nrows);
    dt0->apply_rowindex(ri_neg);
  } else {
    dt0->delete_all();
  }
}


void EvalContext::evaluate_delete_columns() {
  DataTable* dt0 = frames[0].dt;
  auto indices = evaluate_j_as_column_index();
  dt0->delete_columns(indices);
}


void EvalContext::evaluate_delete_subframe() {
  DataTable* dt0 = frames[0].dt;
  const RowIndex& ri0 = frames[0].ri;
  auto indices = evaluate_j_as_column_index();
  for (size_t i : indices) {
    dt0->get_column(i).replace_values(ri0, Column());
  }
}


intvec EvalContext::evaluate_j_as_column_index() {
  expr::Workframe jres = jexpr2.evaluate_j(*this);
  size_t n = jres.size();
  std::vector<size_t> indices(n);
  for (size_t i = 0; i < n; ++i) {
    size_t frame_id, col_id;
    if (jres.is_reference_column(i, &frame_id, &col_id)) {
      if (frame_id == 0) {
        indices[i] = col_id;
        continue;
      }
      throw TypeError() << "Item " << i << " in the `j` selector list is a "
          "column from a joined frame and cannot be deleted";
    }
    if (jres.is_computed_column(i)) {
      throw TypeError() << "Item " << i << " in the `j` selector list is a "
          "computed expression and cannot be deleted";
    }
  }
  return indices;
}



// After evaluation of the j node, the columns in `columns` may have different
// sizes: some are aggregated to group level, others have same number of rows
// as dt0. If this happens, we need to expand the "short" columns to the full
// size.
void EvalContext::fix_columns() {
  if (groupby_mode != GroupbyMode::GtoALL) return;
  xassert(byexpr);
  // size_t nrows0 = get_datatable(0)->nrows;
  size_t ngrps = gb.ngroups();
  RowIndex ungroup_ri;

  for (size_t i = 0; i < columns.size(); ++i) {
    if (columns[i].nrows() != ngrps) continue;
    if (!ungroup_ri) {
      ungroup_ri = gb.ungroup_rowindex();
    }
    const RowIndex& col_rowindex = columns[i]->rowindex();
    columns[i]->replace_rowindex(_product(ungroup_ri, col_rowindex));
  }
}


py::oobj EvalContext::get_result() {
  if (mode == EvalMode::SELECT) {
    DataTable* result =
        out_datatable? out_datatable.release()
                     : new DataTable(std::move(columns), std::move(colnames));
    if (result->ncols == 0) {
      // When selecting a 0-column subset, make sure the number of rows is the
      // same as if some of the columns were selected.
      result->nrows = frames[0].ri? frames[0].ri.size()
                                  : frames[0].dt->nrows;
    }
    return py::Frame::oframe(result);
  }
  return py::None();
}


DataTable* EvalContext::get_datatable(size_t i) const {
  return frames[i].dt;
}


const RowIndex& EvalContext::get_rowindex(size_t i) const {
  return frames[i].ri;
}


const Groupby& EvalContext::get_groupby() {
  return gb;
}


const by_node& EvalContext::get_by_node() const {
  return byexpr;
}


bool EvalContext::is_naturally_joined(size_t i) const {
  return frames[i].natural;
}

bool EvalContext::has_groupby() const {
  return bool(byexpr);
}


size_t EvalContext::nframes() const {
  return frames.size();
}


size_t EvalContext::nrows() const {
  const RowIndex& ri0 = frames[0].ri;
  return ri0? ri0.size() : frames[0].dt->nrows;
}


void EvalContext::apply_rowindex(const RowIndex& ri) {
  for (size_t i = 0; i < frames.size(); ++i) {
    frames[i].ri = ri * frames[i].ri;
  }
}


void EvalContext::apply_groupby(const Groupby& gb_) {
  xassert(static_cast<size_t>(gb_.offsets_r()[gb_.ngroups()]) == nrows());
  gb = gb_;
}



//---- Construct the resulting frame -------------

size_t EvalContext::size() const noexcept {
  return columns.size();
}


void EvalContext::reserve(size_t n) {
  size_t nn = n + columns.size();
  columns.reserve(nn);
  colnames.reserve(nn);
}


void EvalContext::add_column(
  Column&& col, const RowIndex& ri, std::string&& name)
{
  if (ri) {
    const RowIndex& ricol = col->rowindex();
    col->replace_rowindex(_product(ri, ricol));
  }
  columns.push_back(std::move(col));
  colnames.push_back(std::move(name));
}


RowIndex& EvalContext::_product(const RowIndex& ra, const RowIndex& rb) {
  for (auto it = all_ri.rbegin(); it != all_ri.rend(); ++it) {
    if (it->ab == ra && it->bc == rb) {
      return it->ac;
    }
  }
  all_ri.push_back({ra, rb, ra * rb});
  return all_ri.back().ac;
}



} // namespace dt
