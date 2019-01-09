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
#include "expr/base_expr.h"
#include "expr/collist.h"
#include "expr/j_node.h"
#include "expr/workframe.h"   // dt::workframe
namespace dt {


class col_set {
  private:
    using ripair = std::pair<RowIndex, RowIndex>;
    colvec columns;
    std::vector<ripair> all_ri;

  public:
    void reserve_extra(size_t n) {
      columns.reserve(columns.size() + n);
    }
    void add_column(const Column* col, const RowIndex& ri) {
      const RowIndex& ricol = col->rowindex();
      Column* newcol = col->shallowcopy(rowindex_product(ri, ricol));
      columns.push_back(newcol);
    }
    colvec&& release() {
      return std::move(columns);
    }

  private:
    RowIndex& rowindex_product(const RowIndex& ra, const RowIndex& rb) {
      for (auto it = all_ri.rbegin(); it != all_ri.rend(); ++it) {
        if (it->first == ra) {
          return it->second;
        }
      }
      all_ri.push_back(std::make_pair(ra, ra * rb));
      return all_ri.back().second;
    }
};




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
    DataTable* select(workframe&) override;
    void delete_(workframe&) override;
};


DataTable* allcols_jn::select(workframe& wf) {
  col_set cols;
  strvec names;
  for (size_t i = 0; i < wf.nframes(); ++i) {
    const DataTable* dti = wf.get_datatable(i);
    const RowIndex& rii = wf.get_rowindex(i);

    size_t j0 = wf.is_naturally_joined(i)? dti->get_nkeys() : 0;
    cols.reserve_extra(dti->ncols - j0);
    if (wf.nframes() > 1) {
      const strvec& dti_names = dti->get_names();
      names.insert(names.end(),
                   dti_names.begin() + static_cast<long>(j0),
                   dti_names.end());
    }

    for (size_t j = j0; j < dti->ncols; ++j) {
      cols.add_column(dti->columns[j], rii);
    }
  }
  if (wf.nframes() == 1) {
    // Copy names from the source DataTable
    return new DataTable(cols.release(), wf.get_datatable(0));
  }
  else {
    // Otherwise the names must be potentially de-duplicated
    return new DataTable(cols.release(), std::move(names));
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
    DataTable* select(workframe& wf) override;
    void delete_(workframe&) override;
};


collist_jn::collist_jn(cols_intlist* x)
  : indices(std::move(x->indices)), names(std::move(x->names))
{
  xassert(names.empty() || names.size() == indices.size());
}


DataTable* collist_jn::select(workframe& wf) {
  const DataTable* dt0 = wf.get_datatable(0);
  const RowIndex& ri0 = wf.get_rowindex(0);
  if (names.empty()) {
    const strvec& dt0_names = dt0->get_names();
    names.reserve(indices.size());
    for (size_t i : indices) {
      names.push_back(dt0_names[i]);
    }
  }
  col_set cols;
  cols.reserve_extra(indices.size());
  for (size_t i : indices) {
    cols.add_column(dt0->columns[i], ri0);
  }
  return new DataTable(cols.release(), std::move(names));
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
    DataTable* select(workframe& wf) override;
    void delete_(workframe&) override;
};


exprlist_jn::exprlist_jn(cols_exprlist* x)
  : exprs(std::move(x->exprs)), names(std::move(x->names))
{
  xassert(names.empty() || names.size() == exprs.size());
}


DataTable* exprlist_jn::select(workframe& wf) {
  if (names.empty()) {
    names.resize(exprs.size());
  }
  col_set cols;
  cols.reserve_extra(exprs.size());
  for (auto& expr : exprs) {
    expr->resolve(wf);
  }
  RowIndex ri0;  // empty rowindex
  for (auto& expr : exprs) {
    cols.add_column(expr->evaluate_eager(wf), ri0);
  }
  return new DataTable(cols.release(), std::move(names));
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

void j_node::update(workframe&) {}



}  // namespace dt
