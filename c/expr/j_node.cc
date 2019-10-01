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
#include <numeric>            // std::iota
#include "expr/expr.h"
#include "expr/expr_column.h"
#include "expr/collist.h"
#include "expr/j_node.h"
#include "expr/repl_node.h"
#include "expr/eval_context.h"   // dt::EvalContext
#include "datatablemodule.h"
namespace dt {




//------------------------------------------------------------------------------
// allcols_jnode
//------------------------------------------------------------------------------

/**
 * j_node representing selection of all columns (i.e. `:`). This is roughly
 * to equivalent SQL's "*".
 *
 * select()
 *   In the simplest case, this node selects all columns from the source Frame.
 *
 *   The groupby field, if present, has the effect of rearranging the rows
 *   to form contiguous groups. The columns are rearranged so that the group-by
 *   columns are at the front of the frame. The names of the columns will be
 *   exactly the same as in the source frame.
 *
 *   When 2 or more frames are joined, this selector will select all columns
 *   from all joined Frames, with the exception of natural joins, where the key
 *   columns of joined Frames will be excluded from the result.
 *
 * delete_()
 *   Even if several frames are joined, the delete() operator applies only to
 *   the "main" subframe.
 *   When `j` expression selects all columns, the delete() operator removes
 *   the rows from a Frame. This is achieved by computing the Rowindex implied
 *   by the `i` expression, then negating that Rowindex and applying it to the
 *   source frame.
 *   However, when `i` is "all rows", then deleting all rows + all columns
 *   completely empties the Frame: its shape becomes [0 x 0].
 */
class allcols_jnode : public j_node {
  public:
    allcols_jnode() = default;
    GroupbyMode get_groupby_mode(EvalContext&) override;
    void select(EvalContext&) override;
    void delete_(EvalContext&) override;
    void update(EvalContext&, repl_node*) override;
};


GroupbyMode allcols_jnode::get_groupby_mode(EvalContext&) {
  return GroupbyMode::GtoALL;
}


void allcols_jnode::select(EvalContext& ctx) {
  for (size_t i = 0; i < ctx.nframes(); ++i) {
    const DataTable* dti = ctx.get_datatable(i);
    const RowIndex& rii = ctx.get_rowindex(i);
    const strvec& dti_column_names = dti->get_names();
    size_t ncolsi = dti->ncols;

    size_t j0 = ctx.is_naturally_joined(i)? dti->get_nkeys() : 0;
    ctx.reserve(ncolsi - j0);
    const by_node& by = ctx.get_by_node();
    for (size_t j = j0; j < ncolsi; ++j) {
      if (by.has_group_column(j)) continue;
      ctx.add_column(Column(dti->get_column(j)),  // copy
                    rii,
                    std::string(dti_column_names[j]));
    }
  }
}


void allcols_jnode::delete_(EvalContext& ctx) {
  DataTable* dt0 = ctx.get_datatable(0);
  const RowIndex& ri0 = ctx.get_rowindex(0);
  if (ri0) {
    RowIndex ri_neg = ri0.negate(dt0->nrows);
    dt0->apply_rowindex(ri_neg);
  } else {
    dt0->delete_all();
  }
}


void allcols_jnode::update(EvalContext& ctx, repl_node* repl) {
  DataTable* dt0 = ctx.get_datatable(0);
  const RowIndex& ri0 = ctx.get_rowindex(0);
  size_t ncols = dt0->ncols;
  size_t nrows = ri0? ri0.size() : dt0->nrows;
  repl->check_compatibility(nrows, ncols);

  std::vector<size_t> indices(ncols);
  std::iota(indices.begin(), indices.end(), 0);
  if (ri0) {
    repl->replace_values(ctx, indices);
  } else {
    repl->replace_columns(ctx, indices);
  }
}




//------------------------------------------------------------------------------
// simplelist_jnode
//------------------------------------------------------------------------------

/**
 * This is a j node representing a plain selection of columns from the source
 * frame. This node cannot be used to select columns from any joined frames
 * (an `exprlist_jnode` is used in that case).
 *
 * select()
 *   The columns at specific `indices` are selected into a new DataTable. The
 *   RowIndex, if any, is applied to all these columns. The joined frames are
 *   ignored, as well as any groupby information.
 *
 * delete_()
 *   When `i` node is `allrows_in`, then the columns at given indices are
 *   deleted (the indices should also be deduplicated). Otherwise, the
 *   deletion region is a subset of rows/columns, and we just set the values
 *   at those places to NA.
 *
 */
class simplelist_jnode : public j_node {
  private:
    std::vector<size_t> indices;
    strvec names;

  public:
    explicit simplelist_jnode(collist&&);
    GroupbyMode get_groupby_mode(EvalContext&) override;
    void select(EvalContext&) override;
    void delete_(EvalContext&) override;
    void update(EvalContext&, repl_node*) override;

  private:
    void _init_names(EvalContext&);
};


simplelist_jnode::simplelist_jnode(collist&& x)
  : indices(x.release_indices()),
    names(x.release_names())
{
  xassert(names.empty() || names.size() == indices.size());
}


GroupbyMode simplelist_jnode::get_groupby_mode(EvalContext&) {
  return GroupbyMode::GtoALL;
}


void simplelist_jnode::select(EvalContext& ctx) {
  const DataTable* dt0 = ctx.get_datatable(0);
  const RowIndex& ri0 = ctx.get_rowindex(0);
  size_t n = indices.size();

  _init_names(ctx);
  xassert(names.size() == n);

  ctx.reserve(n);
  for (size_t i = 0; i < n; ++i) {
    size_t j = indices[i];
    Column newcol = dt0->get_column(j);  // copy
    ctx.add_column(std::move(newcol), ri0, std::move(names[i]));
  }
}


void simplelist_jnode::delete_(EvalContext& ctx) {
  DataTable* dt0 = ctx.get_datatable(0);
  const RowIndex& ri0 = ctx.get_rowindex(0);
  if (ri0) {
    for (size_t i : indices) {
      dt0->get_column(i).replace_values(ri0, Column());
    }
  } else {
    dt0->delete_columns(indices);
  }
}


void simplelist_jnode::_init_names(EvalContext& ctx) {
  if (!names.empty()) return;
  const strvec& dt0_names = ctx.get_datatable(0)->get_names();
  names.reserve(indices.size());
  for (size_t i : indices) {
    names.push_back(dt0_names[i]);
  }
}


void simplelist_jnode::update(EvalContext& ctx, repl_node* repl) {
  DataTable* dt0 = ctx.get_datatable(0);
  const RowIndex& ri0 = ctx.get_rowindex(0);
  size_t lcols = indices.size();
  size_t lrows = ri0? ri0.size() : dt0->nrows;
  repl->check_compatibility(lrows, lcols);

  size_t ncols = dt0->ncols;
  strvec new_names{ dt0->get_names() };  // copy names
  try {
    size_t num_new_columns = 0;
    for (size_t j : indices) {
      num_new_columns += (j == size_t(-1));
    }
    if (num_new_columns) {
      // Resolve the `repl` node before any changes to `dt0` are committed.
      repl->resolve(ctx);
      new_names.reserve(ncols + num_new_columns);
      for (size_t i = 0; i < indices.size(); ++i) {
        if (indices[i] == size_t(-1)) {
          indices[i] = new_names.size();
          new_names.push_back(names[i]);
        }
      }
      dt0->ncols = new_names.size();
      dt0->set_names(new_names);
    }

    if (ri0) {
      repl->replace_values(ctx, indices);
    } else {
      repl->replace_columns(ctx, indices);
    }
  } catch (...) {
    new_names.resize(ncols);
    dt0->ncols = ncols;
    dt0->set_names(new_names);
    throw;
  }
}




//------------------------------------------------------------------------------
// exprlist_jn
//------------------------------------------------------------------------------
using exprvec = std::vector<std::unique_ptr<dt::expr::base_expr>>;

class exprlist_jn : public j_node {
  private:
    exprvec exprs;
    strvec names;

  public:
    explicit exprlist_jn(collist&&);
    GroupbyMode get_groupby_mode(EvalContext&) override;
    void select(EvalContext&) override;
    void delete_(EvalContext&) override;
    void update(EvalContext&, repl_node*) override;

  private:
    void _init_names(EvalContext&);
};


exprlist_jn::exprlist_jn(collist&& x)
  : exprs(x.release_exprs()),
    names(x.release_names())
{
  xassert(names.empty() || names.size() == exprs.size());
}


GroupbyMode exprlist_jn::get_groupby_mode(EvalContext& ctx) {
  for (auto& expr : exprs) {
    GroupbyMode gm = expr->get_groupby_mode(ctx);
    if (gm == GroupbyMode::GtoALL) return gm;
  }
  return GroupbyMode::GtoONE;
}


void exprlist_jn::select(EvalContext& ctx) {
  _init_names(ctx);
  for (auto& expr : exprs) {
    expr->resolve(ctx);
  }
  size_t n = exprs.size();
  xassert(names.size() == n);

  ctx.reserve(n);
  RowIndex ri0;  // empty rowindex
  for (size_t i = 0; i < n; ++i) {
    auto col = exprs[i]->evaluate(ctx);
    ctx.add_column(std::move(col), ri0, std::move(names[i]));
  }
}


void exprlist_jn::delete_(EvalContext& ctx) {
  for (size_t i = 0; i < exprs.size(); ++i) {
    auto colexpr = dynamic_cast<dt::expr::expr_column*>(exprs[i].get());
    if (!colexpr) {
      throw TypeError() << "Item " << i << " in the `j` selector list is a "
        "computed expression and cannot be deleted";
    }
    if (colexpr->get_col_frame(ctx) > 0) {
      throw TypeError() << "Item " << i << " in the `j` selector list is a "
        "column from a joined frame and cannot be deleted";
    }
  }
  // An `exprlist_jn` cannot contain all exprs that are `expr_column`s and their
  // frame_id is 0. Such node should have been created as `simplelist_jnode` instead.
  xassert(false);  // LCOV_EXCL_LINE
}


void exprlist_jn::_init_names(EvalContext&) {
  if (!names.empty()) return;
  // For now, use empty names. TODO: do something smarter?
  names.resize(exprs.size());
}


void exprlist_jn::update(EvalContext&, repl_node*) {
  throw ValueError() << "Cannot execute an update on computed columns";
}




//------------------------------------------------------------------------------
// j_node
//------------------------------------------------------------------------------

j_node_ptr j_node::make(py::robj src, EvalContext& ctx) {
  // The most common case is ":", a trivial slice
  if ((src.is_slice() && src.to_oslice().is_trivial())
      || src.is_none() || src.is_ellipsis()) {
    return j_node_ptr(new allcols_jnode());
  }
  collist cl(ctx, src, collist::J_NODE);
  return cl.is_simple_list()
            ? j_node_ptr(new simplelist_jnode(std::move(cl)))
            : j_node_ptr(new exprlist_jn(std::move(cl)));
}


j_node::j_node() {
  TRACK(this, sizeof(*this), "j_node");
}

j_node::~j_node() {
  UNTRACK(this);
}




}  // namespace dt
