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
#include "expr/expr_column.h"
#include "utils/exceptions.h"
namespace dt {
namespace expr {



//------------------------------------------------------------------------------
// expr_column
//------------------------------------------------------------------------------

expr_column::expr_column(size_t dfid, py::robj col)
  : frame_id(dfid),
    col_id(size_t(-1)),
    col_selector(col) {}


size_t expr_column::get_frame_id() const noexcept {
  return frame_id;
}


bool expr_column::is_column_expr() const {
  return true;
}


size_t expr_column::get_col_index(const workframe& wf) {
  if (col_id == size_t(-1)) {
    if (frame_id >= wf.nframes()) {
      throw ValueError()
          << "Column expression references a non-existing join frame";
    }
    const DataTable* dt = wf.get_datatable(frame_id);
    if (col_selector.is_int()) {
      int64_t icolid = col_selector.to_int64_strict();
      int64_t incols = static_cast<int64_t>(dt->ncols);
      if (icolid < -incols || icolid >= incols) {
        throw ValueError() << "Column index " << icolid << " is invalid for "
            "a Frame with " << incols << " column" << (incols == 1? "" : "s");
      }
      if (icolid < 0) icolid += incols;
      col_id = static_cast<size_t>(icolid);
    }
    else if (col_selector.is_string()) {
      col_id = dt->xcolindex(col_selector);
    }
    xassert(col_id < dt->ncols);
  }
  return col_id;
}


SType expr_column::resolve(const workframe& wf) {
  size_t i = get_col_index(wf);
  const DataTable* dt = wf.get_datatable(frame_id);
  return dt->get_column(i)->stype();
}


GroupbyMode expr_column::get_groupby_mode(const workframe& wf) const {
  return (frame_id == 0 &&
          wf.has_groupby() &&
          wf.get_by_node().has_group_column(col_id))? GroupbyMode::GtoONE
                                                    : GroupbyMode::GtoALL;
}


colptr expr_column::evaluate_eager(workframe& wf) {
  const DataTable* dt = wf.get_datatable(frame_id);
  const Column* rcol = dt->get_column(col_id);
  const RowIndex& dt_ri = wf.get_rowindex(frame_id);
  const RowIndex& col_ri = rcol->rowindex();

  if (dt_ri) {
    return colptr(rcol->shallowcopy(wf._product(dt_ri, col_ri)));
  }
  else {
    return colptr(rcol->shallowcopy());
  }
}



}} // namespace dt::expr
