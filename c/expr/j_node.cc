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
#include "expr/base_expr.h"
#include "expr/collist.h"
#include "expr/j_node.h"
#include "expr/repl_node.h"
#include "expr/workframe.h"   // dt::workframe
namespace dt {




//------------------------------------------------------------------------------
// allcols_jn
//------------------------------------------------------------------------------

/**
 * j_node representing selection of all columns (i.e. `:`). This is roughly
 * the equivalent of SQL's "*".
 *
 * select()
 *   In the simplest case, this node selects all columns from the source Frame.
 *   The groupby field, if present, is ignored and the columns are selected
 *   as-is, applying the RowIndex that was already computed. The names of the
 *   selected columns will be exactly the same as in the source Frame.
 *
 *   However, when 2 or more Frames are joined, this selector will select all
 *   columns from all joined Frames. The exception to this are natural joins,
 *   where the key columns of joined Frames will be excluded from the result.
 *
 * delete()
 *   Even if several frames are joined, the delete() operator applies only to
 *   the "main" subframe.
 *   When `j` expression selects all columns, the delete() operator removes
 *   the rows from a Frame. This is achieved by computing the Rowindex implied
 *   by the `i` expression, then negating that Rowindex and applying it to the
 *   source frame.
 *   However, when `i` is "all rows", then deleting all rows + all columns
 *   completely empties the Frame: its shape becomes [0 x 0].
 */
class allcols_jn : public j_node {
  public:
    allcols_jn() = default;
    GroupbyMode get_groupby_mode(workframe&) override;
    void select(workframe&) override;
    void delete_(workframe&) override;
    void update(workframe&, repl_node*) override;
};


GroupbyMode allcols_jn::get_groupby_mode(workframe&) {
  return GroupbyMode::GtoALL;
}


void allcols_jn::select(workframe& wf) {
  for (size_t i = 0; i < wf.nframes(); ++i) {
    const DataTable* dti = wf.get_datatable(i);
    const RowIndex& rii = wf.get_rowindex(i);
    const strvec& dti_names = dti->get_names();

    size_t j0 = wf.is_naturally_joined(i)? dti->get_nkeys() : 0;
    wf.reserve(dti->ncols - j0);
    const by_node& by = wf.get_by_node();
    for (size_t j = j0; j < dti->ncols; ++j) {
      if (by.has_group_column(j)) continue;
      wf.add_column(dti->columns[j], rii, std::string(dti_names[j]));
    }
  }
}


void allcols_jn::delete_(workframe& wf) {
  DataTable* dt0 = wf.get_datatable(0);
  const RowIndex& ri0 = wf.get_rowindex(0);
  if (ri0) {
    RowIndex ri_neg = ri0.negate(dt0->nrows);
    dt0->apply_rowindex(ri_neg);
  } else {
    dt0->delete_all();
  }
}


void allcols_jn::update(workframe& wf, repl_node* repl) {
  DataTable* dt0 = wf.get_datatable(0);
  const RowIndex& ri0 = wf.get_rowindex(0);
  size_t ncols = dt0->ncols;
  size_t nrows = ri0? ri0.size() : dt0->nrows;
  repl->check_compatibility(nrows, ncols);

  std::vector<size_t> indices(ncols);
  std::iota(indices.begin(), indices.end(), 0);
  if (ri0) {
    repl->replace_values(wf, indices);
  } else {
    repl->replace_columns(wf, indices);
  }
}




//------------------------------------------------------------------------------
// collist_jn
//------------------------------------------------------------------------------

/**
 * This is a j node representing a plain selection of columns from the source
 * Frame. This node cannot be used to select columns from any joined frames
 * (although those are still allowed in the evaluation graph).
 *
 * select()
 *   The columns at stored indices are selected into a new DataTable. The
 *   RowIndex, if any, is applied to all these columns. The joined frames are
 *   ignored, as well as any groupby information.
 *
 * delete()
 *   When `i` node is `allrows_in`, then the columns at given indices are
 *   deleted (the indices should also be deduplicated). Otherwise, the
 *   deletion region is a subset of rows/columns, and we just set the values
 *   at those places to NA.
 *
 */
class collist_jn : public j_node {
  private:
    std::vector<size_t> indices;
    strvec names;

  public:
    collist_jn(cols_intlist*);
    GroupbyMode get_groupby_mode(workframe&) override;
    void select(workframe&) override;
    void delete_(workframe&) override;
    void update(workframe&, repl_node*) override;

  private:
    void _init_names(workframe&);
};


collist_jn::collist_jn(cols_intlist* x)
  : indices(std::move(x->indices)), names(std::move(x->names))
{
  xassert(names.empty() || names.size() == indices.size());
}


GroupbyMode collist_jn::get_groupby_mode(workframe&) {
  return GroupbyMode::GtoALL;
}


void collist_jn::select(workframe& wf) {
  const DataTable* dt0 = wf.get_datatable(0);
  const RowIndex& ri0 = wf.get_rowindex(0);
  size_t n = indices.size();

  _init_names(wf);
  xassert(names.size() == n);

  wf.reserve(n);
  for (size_t i = 0; i < n; ++i) {
    size_t j = indices[i];
    wf.add_column(dt0->columns[j], ri0, std::move(names[i]));
  }
}


void collist_jn::delete_(workframe& wf) {
  DataTable* dt0 = wf.get_datatable(0);
  const RowIndex& ri0 = wf.get_rowindex(0);
  if (ri0) {
    for (size_t i : indices) {
      dt0->columns[i]->replace_values(ri0, nullptr);
    }
  } else {
    dt0->delete_columns(indices);
  }
}


void collist_jn::_init_names(workframe& wf) {
  if (!names.empty()) return;
  const strvec& dt0_names = wf.get_datatable(0)->get_names();
  names.reserve(indices.size());
  for (size_t i : indices) {
    names.push_back(dt0_names[i]);
  }
}


void collist_jn::update(workframe& wf, repl_node* repl) {
  DataTable* dt0 = wf.get_datatable(0);
  const RowIndex& ri0 = wf.get_rowindex(0);
  size_t lcols = indices.size();
  size_t lrows = ri0? ri0.size() : dt0->nrows;
  repl->check_compatibility(lrows, lcols);

  size_t num_new_columns = 0;
  for (size_t j : indices) {
    num_new_columns += (j == size_t(-1));
  }
  if (num_new_columns) {
    strvec new_names = dt0->get_names();  // copy names
    new_names.reserve(dt0->ncols + num_new_columns);
    dt0->columns.resize(dt0->ncols + num_new_columns);
    for (size_t i = 0; i < indices.size(); ++i) {
      if (indices[i] == size_t(-1)) {
        indices[i] = dt0->ncols++;
        new_names.push_back(names[i]);
      }
    }
    dt0->set_names(new_names);
  }

  if (ri0) {
    repl->replace_values(wf, indices);
  } else {
    repl->replace_columns(wf, indices);
  }
}




//------------------------------------------------------------------------------
// exprlist_jn
//------------------------------------------------------------------------------
using exprvec = std::vector<std::unique_ptr<dt::base_expr>>;

class exprlist_jn : public j_node {
  private:
    exprvec exprs;
    strvec names;

  public:
    exprlist_jn(cols_exprlist*);
    GroupbyMode get_groupby_mode(workframe&) override;
    void select(workframe&) override;
    void delete_(workframe&) override;
    void update(workframe&, repl_node*) override;

  private:
    void _init_names(workframe&);
};


exprlist_jn::exprlist_jn(cols_exprlist* x)
  : exprs(std::move(x->exprs)), names(std::move(x->names))
{
  xassert(names.empty() || names.size() == exprs.size());
}


GroupbyMode exprlist_jn::get_groupby_mode(workframe& wf) {
  for (auto& expr : exprs) {
    GroupbyMode gm = expr->get_groupby_mode(wf);
    if (gm == GroupbyMode::GtoALL) return gm;
  }
  return GroupbyMode::GtoONE;
}


void exprlist_jn::select(workframe& wf) {
  _init_names(wf);
  for (auto& expr : exprs) {
    expr->resolve(wf);
  }
  size_t n = exprs.size();
  xassert(names.size() == n);

  wf.reserve(n);
  RowIndex ri0;  // empty rowindex
  for (size_t i = 0; i < n; ++i) {
    wf.add_column(exprs[i]->evaluate_eager(wf), ri0, std::move(names[i]));
  }
}


void exprlist_jn::delete_(workframe&) {
  for (size_t i = 0; i < exprs.size(); ++i) {
    auto colexpr = dynamic_cast<dt::expr_column*>(exprs[i].get());
    if (!colexpr) {
      throw TypeError() << "Item " << i << " in the `j` selector list is a "
        "computed expression and cannot be deleted";
    }
    if (colexpr->get_frame_id() > 0) {
      throw TypeError() << "Item " << i << " in the `j` selector list is a "
        "column from a joined frame and cannot be deleted";
    }
  }
  // An `exprlist_jn` cannot contain all exprs that are `expr_column`s and their
  // frame_id is 0. Such node should have been created as `collist_jn` instead.
  xassert(false);  // LCOV_EXCL_LINE
}


void exprlist_jn::_init_names(workframe&) {
  if (!names.empty()) return;
  // For now, use empty names. TODO: do something smarter?
  names.resize(exprs.size());
}


void exprlist_jn::update(workframe&, repl_node*) {
  throw ValueError() << "Cannot execute an update on computed columns";
}




//------------------------------------------------------------------------------
// j_node
//------------------------------------------------------------------------------

j_node_ptr j_node::make(py::robj src, workframe& wf) {
  // The most common case is ":", a trivial slice
  if ((src.is_slice() && src.to_oslice().is_trivial())
      || src.is_none() || src.is_ellipsis()) {
    return j_node_ptr(new allcols_jn());
  }
  collist_ptr cl = collist::make(wf, src, "`j` selector");
  auto cl_int = dynamic_cast<cols_intlist*>(cl.get());
  auto cl_expr = dynamic_cast<cols_exprlist*>(cl.get());
  xassert(cl_int || cl_expr);
  return cl_int? j_node_ptr(new collist_jn(cl_int))
               : j_node_ptr(new exprlist_jn(cl_expr));
}


j_node::~j_node() {}




}  // namespace dt
