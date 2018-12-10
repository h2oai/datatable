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
    void clear_cache() {
      all_ri.clear();
    }
    colvec&& release() {
      return std::move(columns);
    }

  private:
    RowIndex& rowindex_product(const RowIndex& ra, const RowIndex& rb) {
      for (auto it = all_ri.rbegin(); it != all_ri.rend(); ++it) {
        if (it->first == rb) {
          return it->second;
        }
      }
      all_ri.push_back(std::make_pair(rb, ra * rb));
      return all_ri.back().second;
    }
};



//------------------------------------------------------------------------------
// allcols_ji
//------------------------------------------------------------------------------

/**
 * j_node representing selection of all columns.
 */
class allcols_ji : public j_node {
  public:
    allcols_ji() = default;
    DataTable* execute(workframe&) override;
};


DataTable* allcols_ji::execute(workframe& wf) {
  col_set cols;
  strvec names;
  for (size_t i = 0; i < wf.nframes(); ++i) {
    DataTable* dti = wf.get_datatable(i);
    const RowIndex& rii = wf.get_rowindex(i);

    size_t j0 = (i > 0)? dti->get_nkeys() : 0;
    cols.reserve_extra(dti->ncols - j0);
    if (wf.nframes() > 1) {
      const strvec& dti_names = dti->get_names();
      names.insert(names.end(), dti_names.begin(), dti_names.end());
    }

    for (size_t j = j0; j < dti->ncols; ++j) {
      cols.add_column(dti->columns[j], rii);
    }
    cols.clear_cache();
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




//------------------------------------------------------------------------------
// j_node factory function
//------------------------------------------------------------------------------

j_node* j_node::make(py::robj src) {
  // The most common case is `:`, a trivial slice
  if (src.is_slice()) {
    auto ssrc = src.to_oslice();
    if (ssrc.is_trivial()) return new allcols_ji();
    // if (ssrc.is_numeric()) {
    //   return new slice_ii(ssrc.start(), ssrc.stop(), ssrc.step());
    // }
    // throw TypeError() << src << " is not integer-valued";
  }
  // if (is_PyBaseExpr(src)) {
  //   return new expr_ii(src);
  // }
  if (src.is_none() || src.is_ellipsis()) {
    return new allcols_ji();
  }
  if (src.is_bool()) {
    throw TypeError() << "Boolean value cannot be used as a `j` expression";
  }
  return nullptr;  // for now
}


j_node::~j_node() {}



}  // namespace dt
