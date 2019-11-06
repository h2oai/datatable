//------------------------------------------------------------------------------
// Copyright 2018-2019 H2O.ai
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
#include "expr/py_update.h"
#include "frame/py_frame.h"
namespace dt {


//------------------------------------------------------------------------------
// EvalContext
//------------------------------------------------------------------------------

EvalContext::EvalContext(DataTable* dt, EvalMode evalmode) {
  // The source frame must have flag `natural=false` so that `allcols_jn`
  // knows to select all columns from it.
  frames.push_back(subframe {dt, RowIndex(), false});
  mode = evalmode;
  groupby_mode = GroupbyMode::NONE;
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
  iexpr_ = dt::expr::Expr(oi);
}


void EvalContext::add_j(py::oobj oj) {
  xassert(!jexpr);
  py::oupdate arg_update = oj.to_oupdate_lax();
  if (arg_update) {
    if (mode == EvalMode::DELETE) {
      throw ValueError() << "update() clause cannot be used with a "
                            "delete expression";
    }
    if (mode == EvalMode::UPDATE) {
      throw ValueError() << "update() clause cannot be used with an "
                            "assignment expression";
    }
    mode = EvalMode::UPDATE;
    jexpr_ = dt::expr::Expr(arg_update.get_names());
    jexpr = j_node::make(arg_update.get_names(), *this);  // remove
    repl_ = dt::expr::Expr(arg_update.get_exprs());
  } else {
    jexpr_ = dt::expr::Expr(oj);
    jexpr = j_node::make(oj, *this);   // remove
  }
}


void EvalContext::add_replace(py::oobj obj) {
  repl_ = dt::expr::Expr(obj);
}




//------------------------------------------------------------------------------
// Properties
//------------------------------------------------------------------------------

EvalMode EvalContext::get_mode() const {
  return mode;
}

GroupbyMode EvalContext::get_groupby_mode() const {
  return groupby_mode;
}

const RowIndex& EvalContext::get_ungroup_rowindex() {
  if (!ungroup_rowindex_) {
    ungroup_rowindex_ = gb.ungroup_rowindex();
  }
  return ungroup_rowindex_;
}




//------------------------------------------------------------------------------
// Main evaluation
//------------------------------------------------------------------------------

void EvalContext::evaluate() {
  // Compute joins
  DataTable* xdt = frames[0].dt;
  for (size_t i = 1; i < frames.size(); ++i) {
    DataTable* jdt = frames[i].dt;
    frames[i].ri = natural_join(*xdt, *jdt);
  }

  // Compute groupby
  if (byexpr) {
    groupby_mode = jexpr->get_groupby_mode(*this);
  }
  byexpr.execute(*this);

  // Compute i filter
  if (has_groupby()) {
    auto rigb = iexpr_.evaluate_iby(*this);
    apply_rowindex(rigb.first);
    apply_groupby(rigb.second);
  } else {
    RowIndex rowindex = iexpr_.evaluate_i(*this);
    apply_rowindex(rowindex);
  }

  switch (mode) {
    case EvalMode::SELECT:
      if (byexpr) {
        byexpr.create_columns(*this);
        jexpr->select(*this);
        fix_columns();
      }
      else {
        auto res = jexpr_.evaluate_j(*this);
        out_datatable = std::move(res).convert_to_datatable();
      }
      break;

    case EvalMode::DELETE: evaluate_delete(); break;
    case EvalMode::UPDATE: evaluate_update(); break;
  }
}



// Helper for DELETE and UPDATE evaluation: in these modes the
// j expression must be a list of column references, so this
// method will simply return them as a list of colum indices.
//
// Additionally, under UPDATE evaluation mode we allow the j list
// to contain references to unknown columns too. If such "new"
// columns exist, they will be created as empty `Column` objects
// in `dt0`, with the approriate names. The returned array of indices
// will contain the location of these columns in the updated `dt0`.
//
intvec EvalContext::evaluate_j_as_column_index() {
  bool allow_new = (mode == EvalMode::UPDATE);
  DataTable* dt0 = frames[0].dt;
  auto jres = jexpr_.evaluate_j(*this, allow_new);
  size_t n = jres.ncols();
  intvec indices(n);

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
    if (jres.is_placeholder_column(i)) {
      // If allow_new is false, no placeholder columns should be generated
      xassert(allow_new);
      indices[i] = dt0->ncols() + newnames.size();
      newnames.emplace_back(jres.retrieve_name(i));
    }
    if (jres.is_computed_column(i)) {
      throw TypeError() << "Item " << i << " in the `j` selector list is a "
          "computed expression and cannot be deleted";
    }
  }
  return indices;
}


void EvalContext::create_placeholder_columns() {
  if (newnames.empty()) return;
  DataTable* dt0 = frames[0].dt;
  const strvec& oldnames = dt0->get_names();
  newnames.insert(newnames.begin(), oldnames.begin(), oldnames.end());
  dt0->resize_columns(newnames);
}




//------------------------------------------------------------------------------
// DELETE
//------------------------------------------------------------------------------

// Main delete function: `del DT[...]`. Deleting basically falls into
// four categories:
//   - delete rows from a frame;
//   - delete columns from a frame;
//   - delete subset of both rows & columns;
//   - delete all rows & all columns (i.e. delete the entire frame).
//
void EvalContext::evaluate_delete() {
  if (jexpr_.get_expr_kind() == expr::Kind::SliceAll) {
    evaluate_delete_rows();
  } else if (frames[0].ri) {
    evaluate_delete_subframe();
  } else {
    evaluate_delete_columns();
  }
}


// Delete a subset of rows from the frame: `del DT[i, :]`
//
void EvalContext::evaluate_delete_rows() {
  DataTable* dt0 = frames[0].dt;
  const RowIndex& ri0 = frames[0].ri;
  if (ri0) {
    RowIndex ri_neg = ri0.negate(dt0->nrows());
    dt0->apply_rowindex(ri_neg);
  } else {
    dt0->delete_all();
  }
}


// Delete columns from the frame: `del DT[:, j]`
//
void EvalContext::evaluate_delete_columns() {
  DataTable* dt0 = frames[0].dt;
  auto indices = evaluate_j_as_column_index();
  dt0->delete_columns(indices);
}


// Delete a rectangular subset of values. The subset contains neither whole
// rows nor whole columns: `del DT[i, j]`. Deleting these values actually
// replaces them with NAs.
//
void EvalContext::evaluate_delete_subframe() {
  DataTable* dt0 = frames[0].dt;
  const RowIndex& ri0 = frames[0].ri;
  auto indices = evaluate_j_as_column_index();
  if (indices.empty()) return;
  if (dt0->nkeys()) {
    // Check whether replacing values with NAs would be legal before trying
    // to actually do any replacements.
    for (size_t i : indices) {
      if (i < dt0->nkeys()) {
        throw ValueError()
          << "Cannot delete values from key columns in the Frame";
      }
    }
  }
  for (size_t i : indices) {
    dt0->get_column(i).replace_values(ri0, Column());
  }
}




//------------------------------------------------------------------------------
// UPDATE
//------------------------------------------------------------------------------

/**
  * When updating a Frame, there are two main cases to consider:
  *
  *   - `DT[:, j] = R` or `DT[:, :] = R`
  *   - `DT[i, j] = R` or `DT[i, :] = R`
  *
  * In the first case the entire columns are replaced, which means we
  * can allow their stypes to change.
  *
  * In the second case we only replace the values in a subset of rows,
  * and therefore the replacement frame must (loosely) have the same
  * types as the LHS (we *may* relax this requirement in the future).
  *
  */
void EvalContext::evaluate_update() {
  auto dt0 = get_datatable(0);
  auto ri0 = get_rowindex(0);
  auto ncols = dt0->ncols();
  auto nkeys = dt0->nkeys();
  auto indices = evaluate_j_as_column_index();

  std::vector<SType> stypes;
  stypes.reserve(indices.size());
  for (size_t i : indices) {
    stypes.push_back(i < ncols? dt0->get_column(i).stype()
                              : SType::VOID);
    if (i < nkeys) {
      throw ValueError() << "Cannot change values in a key column "
                         << "`" << dt0->get_names()[i] << "`";
    }
  }

  expr::Workframe replacement = repl_.evaluate_r(*this, stypes);
  size_t lrows = nrows();
  size_t lcols = indices.size();
  replacement.reshape_for_update(lrows, lcols);
  create_placeholder_columns();
  typecheck_for_update(replacement, indices);

  if (ri0) {
    for (size_t i = 0; i < lcols; ++i) {
      Column rcol = replacement.retrieve_column(i);
      size_t j = indices[i];
      if (j >= ncols) {  // new column
        dt0->set_column(j, Column::new_na_column(dt0->nrows(), rcol.stype()));
      }
      Column& colj = dt0->get_column(j);
      SType jst = colj.stype();
      SType rst = rcol.stype();
      if (jst != rst) {
        if (static_cast<size_t>(jst) < static_cast<size_t>(rst)) {
          colj.cast_inplace(rst);
        } else {
          rcol.cast_inplace(jst);
        }
      }
      colj.replace_values(ri0, rcol);
    }
  } else {
    for (size_t i = 0; i < lcols; ++i) {
      dt0->set_column(indices[i], replacement.retrieve_column(i));
    }
  }
}


void EvalContext::typecheck_for_update(
    expr::Workframe& replframe, const intvec& indices)
{
  DataTable* dt0 = get_datatable(0);
  bool allrows = !(frames[0].ri);
  bool repl_1row = replframe.get_grouping_mode() == expr::Grouping::SCALAR;
  size_t n = indices.size();
  xassert(replframe.ncols() == indices.size());

  for (size_t i = 0; i < n; ++i) {
    const Column& lcol = dt0->get_column(indices[i]);
    const Column& rcol = replframe.get_column(i);
    if (!lcol || lcol.stype() == SType::VOID) continue;
    if (allrows && !repl_1row) continue; // Keep rcol's type as-is
    if (lcol.stype() != rcol.stype()) {
      auto llt = lcol.ltype();
      auto rlt = rcol.ltype();
      bool ok = (llt == rlt) ||
                (llt == LType::REAL && rlt == LType::INT);
      if (!ok) {
        throw TypeError() << "Cannot assign " << rlt
          << " value to column `" << dt0->get_names()[indices[i]]
          << "` of type " << lcol.stype();
      }
    }
  }
}



//------------------------------------------------------------------------------
// Other
//------------------------------------------------------------------------------

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
    columns[i].apply_rowindex(ungroup_ri);
  }
}


py::oobj EvalContext::get_result() {
  if (mode == EvalMode::SELECT) {
    DataTable* result =
        out_datatable? out_datatable.release()
                     : new DataTable(std::move(columns), std::move(colnames));
    if (result->ncols() == 0) {
      // When selecting a 0-column subset, make sure the number of rows is the
      // same as if some of the columns were selected.
      result->resize_rows(frames[0].ri? frames[0].ri.size()
                                      : frames[0].dt->nrows());
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
  return ri0? ri0.size() : frames[0].dt->nrows();
}


void EvalContext::apply_rowindex(const RowIndex& ri) {
  for (size_t i = 0; i < frames.size(); ++i) {
    frames[i].ri = ri * frames[i].ri;
  }
}


void EvalContext::apply_groupby(const Groupby& gb_) {
  xassert(gb_.last_offset() == nrows());
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
  col.apply_rowindex(ri);
  columns.push_back(std::move(col));
  colnames.push_back(std::move(name));
}



} // namespace dt
