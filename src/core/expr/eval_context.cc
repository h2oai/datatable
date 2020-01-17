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
#include "expr/expr.h"
#include "expr/eval_context.h"
#include "expr/py_by.h"
#include "expr/py_join.h"
#include "expr/py_sort.h"
#include "expr/py_update.h"
#include "expr/workframe.h"
#include "frame/py_frame.h"
#include "sort.h"
namespace dt {
namespace expr {



//------------------------------------------------------------------------------
// EvalContext: construction
//------------------------------------------------------------------------------

EvalContext::EvalContext(DataTable* dt, EvalMode evalmode)
  : groupby_columns_(*this)
{
  frames_.emplace_back(dt, RowIndex(), /* natural_join= */ false);
  eval_mode_ = evalmode;
  add_groupby_columns_ = true;
}



void EvalContext::add_join(py::ojoin oj) {
  DataTable* dt = oj.get_datatable();
  frames_.emplace_back(dt, RowIndex(), /* natural_join= */ true);
}


void EvalContext::add_groupby(py::oby obj) {
  if (byexpr_) {
    throw TypeError() << "Multiple by()'s are not allowed";
  }
  byexpr_ = Expr(obj.get_arguments());
  add_groupby_columns_ = obj.get_add_columns();
}


void EvalContext::add_sortby(py::osort obj) {
  if (sortexpr_) {
    throw TypeError() << "Multiple sort()'s are not allowed";
  }
  sortexpr_ = Expr(obj.get_arguments());
}


void EvalContext::add_i(py::oobj oi) {
  iexpr_ = Expr(oi);
}


void EvalContext::add_j(py::oobj oj) {
  py::oupdate arg_update = oj.to_oupdate_lax();
  if (arg_update) {
    if (eval_mode_ == EvalMode::DELETE) {
      throw ValueError() << "update() clause cannot be used with a "
                            "delete expression";
    }
    if (eval_mode_ == EvalMode::UPDATE) {
      throw ValueError() << "update() clause cannot be used with an "
                            "assignment expression";
    }
    eval_mode_ = EvalMode::UPDATE;
    jexpr_ = Expr(arg_update.get_names());
    rexpr_ = Expr(arg_update.get_exprs());
  } else {
    jexpr_ = Expr(oj);
  }
}


void EvalContext::add_replace(py::oobj obj) {
  rexpr_ = Expr(obj);
}




//------------------------------------------------------------------------------
// Properties
//------------------------------------------------------------------------------

EvalMode EvalContext::get_mode() const {
  return eval_mode_;
}

const RowIndex& EvalContext::get_ungroup_rowindex() {
  if (!ungroup_rowindex_) {
    ungroup_rowindex_ = groupby_.ungroup_rowindex();
  }
  return ungroup_rowindex_;
}

const RowIndex& EvalContext::get_group_rowindex() {
  if (!group_rowindex_) {
    size_t n = groupby_.size();
    if (n == 1 && groupby_.last_offset() == 0) n = 0;
    // TODO: when RowIndex supports Buffers, this could be replaced by a
    //       simple buffer copy from groupby_ into the RowIndex
    arr32_t offsets(n);
    std::memcpy(offsets.data(), groupby_.offsets_r(), n * sizeof(int32_t));
    group_rowindex_ = RowIndex(std::move(offsets), true);
  }
  return group_rowindex_;
}




//------------------------------------------------------------------------------
// Main evaluation
//------------------------------------------------------------------------------

py::oobj EvalContext::evaluate() {
  // Compute joins
  DataTable* xdt = get_datatable(0);
  for (size_t i = 1; i < nframes(); ++i) {
    DataTable* jdt = get_datatable(i);
    frames_[i].ri_ = natural_join(*xdt, *jdt);
  }

  compute_groupby_and_sort();
  xassert(groupby_);

  // Compute i filter
  if (byexpr_ || sortexpr_) {
    auto rigb = iexpr_.evaluate_iby(*this);
    apply_rowindex(std::move(rigb.first));
    replace_groupby(std::move(rigb.second));
  } else {
    RowIndex rowindex = iexpr_.evaluate_i(*this);
    apply_rowindex(std::move(rowindex));
    replace_groupby(Groupby::single_group(nrows()));
  }

  switch (eval_mode_) {
    case EvalMode::SELECT: return evaluate_select();
    case EvalMode::DELETE: return evaluate_delete();
    case EvalMode::UPDATE: return evaluate_update();
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
  bool allow_new = (eval_mode_ == EvalMode::UPDATE);
  DataTable* dt0 = get_datatable(0);
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
      indices[i] = dt0->ncols() + newnames_.size();
      newnames_.emplace_back(jres.retrieve_name(i));
    }
    if (jres.is_computed_column(i)) {
      throw TypeError() << "Item " << i << " in the `j` selector list is a "
          "computed expression and cannot be deleted";
    }
  }
  return indices;
}


void EvalContext::create_placeholder_columns() {
  if (newnames_.empty()) return;
  DataTable* dt0 = get_datatable(0);
  const strvec& oldnames = dt0->get_names();
  newnames_.insert(newnames_.begin(), oldnames.begin(), oldnames.end());
  dt0->resize_columns(newnames_);
}



//------------------------------------------------------------------------------
// Groupby
//------------------------------------------------------------------------------

// Compute groupby: either from the `byexpr_`, or fall-back to
// single group that encompasses the entire frame. Note that this
// single group might be empty if the frame has 0 rows.
//
void EvalContext::compute_groupby_and_sort() {
  size_t nr = nrows();
  if (byexpr_ || sortexpr_) {
    Workframe wf(*this);
    std::vector<Column> cols;
    std::vector<SortFlag> flags;
    size_t n_group_cols = 0;
    if (byexpr_) {
      byexpr_.prepare_by(*this, wf, flags);
      n_group_cols = wf.ncols();
    }
    if (sortexpr_) {
      sortexpr_.prepare_by(*this, wf, flags);
    }
    size_t ncols = wf.ncols();
    xassert(flags.size() == ncols);
    if (ncols) {
      for (size_t i = 0; i < ncols; ++i) {
        const Column& col = wf.get_column(i);
        const_cast<Column&>(col).materialize();
        cols.push_back(col);  // copy
        if (i >= n_group_cols) {
          flags[i] = flags[i] | SortFlag::SORT_ONLY;
        }
      }
      wf.truncate_columns(n_group_cols);
      set_groupby_columns(std::move(wf));

      auto rigb = group(cols, flags);
      apply_rowindex(std::move(rigb.first));
      groupby_ = std::move(rigb.second);
    }
  }
  if (!groupby_) {
    groupby_ = Groupby::single_group(nr);
  }
  xassert(groupby_.last_offset() == nr);
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
py::oobj EvalContext::evaluate_delete() {
  Kind jkind = jexpr_.get_expr_kind();
  if (jkind == Kind::SliceAll) {
    evaluate_delete_rows();
  } else if (jkind == Kind::NamedList) {
    throw TypeError() << "When del operator is applied, `j` selector cannot "
                         "be a dictionary";
  }
  else if (get_rowindex(0)) {
    evaluate_delete_subframe();
  } else {
    evaluate_delete_columns();
  }
  return py::None();
}


// Delete a subset of rows from the frame: `del DT[i, :]`
//
void EvalContext::evaluate_delete_rows() {
  DataTable* dt0 = get_datatable(0);
  const RowIndex& ri0 = get_rowindex(0);
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
  DataTable* dt0 = get_datatable(0);
  auto indices = evaluate_j_as_column_index();
  dt0->delete_columns(indices);
}


// Delete a rectangular subset of values. The subset contains neither whole
// rows nor whole columns: `del DT[i, j]`. Deleting these values actually
// replaces them with NAs.
//
void EvalContext::evaluate_delete_subframe() {
  DataTable* dt0 = get_datatable(0);
  const RowIndex& ri0 = get_rowindex(0);
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
py::oobj EvalContext::evaluate_update() {
  auto dt0 = get_datatable(0);
  auto ri0 = get_rowindex(0);
  auto ncols = dt0->ncols();
  auto nkeys = dt0->nkeys();
  intvec indices = evaluate_j_as_column_index();

  for (size_t i : indices) {
    if (i < nkeys) {
      throw ValueError() << "Cannot change values in a key column "
                         << "`" << dt0->get_names()[i] << "`";
    }
  }

  Workframe replacement = rexpr_.evaluate_r(*this, indices);
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
  return py::None();
}


void EvalContext::typecheck_for_update(
    Workframe& replframe, const intvec& indices)
{
  DataTable* dt0 = get_datatable(0);
  bool allrows = !(get_rowindex(0));
  bool repl_1row = replframe.get_grouping_mode() == Grouping::SCALAR;
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
// SELECT
//------------------------------------------------------------------------------

void EvalContext::update_groupby_columns(Grouping gmode) {
  auto ri0 = get_rowindex(0);
  if (gmode == Grouping::GtoONE) {
    ri0 = get_group_rowindex() * ri0;
  }
  size_t n = groupby_columns_.ncols();
  for (size_t i = 0; i < n; ++i) {
    Column col = groupby_columns_.retrieve_column(i);
    col.apply_rowindex(ri0);
    groupby_columns_.replace_column(i, std::move(col));
  }
  groupby_columns_.grouping_mode_ = gmode;
}


template <typename T>
static void _vivify_column(const Column& col) {
  T value;
  col.get_element(0, &value);
}

// Ensure that any "Latent" columns are resolved before we return
// the Frame to the user.
// Strictly speaking we don't have to resolve them, but then we'd
// have to be careful about accessing columns' data in parallel.
//
static void _vivify_workframe(const Workframe& wf) {
  if (wf.nrows() == 0) return;
  for (size_t i = 0; i < wf.ncols(); ++i) {
    const Column& col = wf.get_column(i);
    switch (col.stype()) {
      case SType::BOOL:
      case SType::INT8:    _vivify_column<int8_t>(col); break;
      case SType::INT16:   _vivify_column<int16_t>(col); break;
      case SType::INT32:   _vivify_column<int32_t>(col); break;
      case SType::INT64:   _vivify_column<int64_t>(col); break;
      case SType::FLOAT32: _vivify_column<float>(col); break;
      case SType::FLOAT64: _vivify_column<double>(col); break;
      case SType::STR32:
      case SType::STR64:   _vivify_column<CString>(col); break;
      case SType::OBJ:     _vivify_column<py::robj>(col); break;
      default:
        throw RuntimeError() << "Unknown stype " << col.stype();  // LCOV_EXCL_LINE
    }
  }
}


py::oobj EvalContext::evaluate_select() {
  Workframe res = jexpr_.evaluate_j(*this);
  if (add_groupby_columns_) {
    update_groupby_columns(res.get_grouping_mode());
    res.cbind(std::move(groupby_columns_), /* at_end= */ false);
  }
  _vivify_workframe(res);

  auto result = std::move(res).convert_to_datatable();
  if (result->ncols() == 0) {
    // When selecting a 0-column subset, make sure the number of rows is the
    // same as if some of the columns were selected.
    result->resize_rows(nrows());
  }
  return py::Frame::oframe(result.release());
}




//------------------------------------------------------------------------------
// Other
//------------------------------------------------------------------------------

DataTable* EvalContext::get_datatable(size_t i) const {
  return frames_[i].dt_;
}


const RowIndex& EvalContext::get_rowindex(size_t i) const {
  return frames_[i].ri_;
}


const Groupby& EvalContext::get_groupby() {
  return groupby_;
}


bool EvalContext::is_naturally_joined(size_t i) const {
  return frames_[i].natural_;
}

bool EvalContext::has_groupby() const {
  return bool(byexpr_);
}


bool EvalContext::has_group_column(size_t frame_index, size_t col_index) const
{
  size_t n = groupby_columns_.ncols();
  size_t iframe, icol;
  for (size_t i = 0; i < n; ++i) {
    if (groupby_columns_.is_reference_column(i, &iframe, &icol)) {
      if (iframe == frame_index && icol == col_index) return true;
    }
  }
  return false;
}


size_t EvalContext::nframes() const {
  return frames_.size();
}


size_t EvalContext::nrows() const {
  const RowIndex& ri0 = get_rowindex(0);
  return ri0? ri0.size() : get_datatable(0)->nrows();
}


void EvalContext::apply_rowindex(const RowIndex& ri) {
  if (!ri) return;
  for (size_t i = 0; i < nframes(); ++i) {
    frames_[i].ri_ = ri * get_rowindex(i);
  }
}


void EvalContext::replace_groupby(Groupby&& gb) {
  if (gb) {
    xassert(gb.last_offset() == nrows());
    groupby_ = std::move(gb);
  }
}


void EvalContext::set_groupby_columns(Workframe&& wf) {
  groupby_columns_.cbind(std::move(wf));
}




}} // namespace dt::expr
