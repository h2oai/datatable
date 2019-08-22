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
#ifndef dt_EXPR_EXPR_COLUMN_h
#define dt_EXPR_EXPR_COLUMN_h
#include "expr/expr.h"
#include "python/_all.h"
namespace dt {
namespace expr {


class expr_column : public base_expr {
  private:
    size_t frame_id;
    size_t col_id;
    py::oobj col_selector;

  public:
    expr_column(size_t dfid, py::robj col);
    expr_column(size_t dfid, size_t colid);

    // get_col_index(wf, strict)
    //   Resolve the column-selector expression within the context of the given
    //   workframe, and return the index of the column within its frame. The
    //   `strict` flag controls the behavior in case the column cannot be found
    //   in the frame: when the flag is true an exception is raised; otherwise
    //   merely returns -1.
    //
    size_t get_col_index(const workframe&, bool strict = true);

    // get_col_frame(wf)
    //   Return the index of this column's frame within the workframe.
    //
    size_t get_col_frame(const workframe&);

    SType resolve(const workframe&) override;
    GroupbyMode get_groupby_mode(const workframe&) const override;
    Column evaluate_eager(workframe&) override;
};


}}
#endif
