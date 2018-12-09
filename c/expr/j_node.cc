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


//------------------------------------------------------------------------------
// jnode_impl
//------------------------------------------------------------------------------

class jnode_impl {
  public:
    virtual ~jnode_impl();
    virtual DataTable* execute(workframe& wf) = 0;
};

jnode_impl::~jnode_impl() {}




//------------------------------------------------------------------------------
// allcols_ji
//------------------------------------------------------------------------------

/**
 * j_node representing selection of all columns.
 */
class allcols_ji : public jnode_impl {
  public:
    allcols_ji() = default;
    virtual DataTable* execute(workframe&) override;
};


DataTable* allcols_ji::execute(workframe& wf) {
  colvec cols;
  strvec names;
  for (size_t i = 0; i < wf.nframes(); ++i) {
    DataTable* dti = wf.get_datatable(i);
    const RowIndex& rii = wf.get_rowindex(i);

    size_t j0 = (i > 0)? dti->get_nkeys() : 0;
    cols.reserve(cols.size() + dti->ncols - j0);
    if (wf.nframes() > 1) {
      const strvec& dti_names = dti->get_names();
      names.insert(names.end(), dti_names.begin(), dti_names.end());
    }

    for (size_t j = j0; j < dti->ncols; ++j) {
      Column* colj = dti->columns[j];
      const RowIndex& rij = colj->rowindex();

      cols.push_back(colj->shallowcopy(rii * rij));
    }
  }
  if (wf.nframes() == 1) {
    // Copy names from the source DataTable
    return new DataTable(std::move(cols), wf.get_datatable(0));
  }
  else {
    // Otherwise the names must be potentially de-duplicated
    return new DataTable(std::move(cols), std::move(names));
  }
}




//------------------------------------------------------------------------------
// j_node factory function
//------------------------------------------------------------------------------

static jnode_impl* _make_impl(py::robj src) {
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




//------------------------------------------------------------------------------
// j_node API
//------------------------------------------------------------------------------

j_node::j_node(py::robj src)
  : impl(_make_impl(src)) {}



}  // namespace dt
