//------------------------------------------------------------------------------
// Copyright 2019 H2O.ai
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
#ifndef dt_EXPR_HEAD_FRAME_h
#define dt_EXPR_HEAD_FRAME_h
#include <string>
#include <vector>
#include "expr/head.h"
namespace dt {
namespace expr {


/**
  * Head corresponding to a datatable Frame being used in a DT[i,j]
  * expression. This class is also used when there is a numpy array
  * or a pandas DataFrame.
  *
  * The field `container_` holds python object that owns the Frame,
  * thus ensuring that the `DataTable*` pointer `dt_` remains valid.
  *
  * The flag `ignore_names_` is set when the class is create from a
  * numpy array, since the numpy array has no column names.
  */
class Head_Frame : public Head {
  private:
    py::oobj container_;
    DataTable* dt_;
    bool ignore_names_;
    size_t : 56;

  public:
    static ptrHead from_datatable(py::robj src);
    static ptrHead from_numpy(py::robj src);
    static ptrHead from_pandas(py::robj src);

    Head_Frame(py::robj src, bool ignore_names = false);
    Kind get_expr_kind() const override;

    Workframe evaluate_n(const vecExpr&, EvalContext&, bool) const override;
    Workframe evaluate_j(const vecExpr&, EvalContext&, bool) const override;
    Workframe evaluate_r(const vecExpr&, EvalContext&, const intvec&) const override;
    Workframe evaluate_f(EvalContext&, size_t, bool) const override;
    RowIndex  evaluate_i(const vecExpr&, EvalContext&) const override;
    RiGb      evaluate_iby(const vecExpr&, EvalContext&) const override;
};




}}  // namespace dt::expr
#endif
