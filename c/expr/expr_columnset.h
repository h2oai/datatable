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
#ifndef dt_EXPR_EXPR_COLUMNSET_h
#define dt_EXPR_EXPR_COLUMNSET_h
#include <vector>
#include "expr/collist.h"
#include "expr/expr.h"
#include "python/_all.h"
namespace dt {
namespace expr {


class expr_columnset : public base_expr {
  public:
    expr_columnset() = default;
    expr_columnset(const expr_columnset&) = delete;
    expr_columnset(expr_columnset&&) = delete;

    virtual collist_ptr convert_to_collist(workframe&, size_t flags) = 0;

    bool is_columnset_expr() const override;
    SType resolve(const workframe&) override;
    GroupbyMode get_groupby_mode(const workframe&) const override;
    OColumn evaluate_eager(workframe&) override;
};


class expr_simple_columnset : public expr_columnset {
  private:
    size_t frame_id;
    py::oobj selector;

  public:
    expr_simple_columnset(size_t frid, py::robj arg);
    collist_ptr convert_to_collist(workframe&, size_t flags) override;
};


class expr_singlecol_columnset : public expr_columnset {
  private:
    pexpr arg;

  public:
    expr_singlecol_columnset(pexpr&& a);
    collist_ptr convert_to_collist(workframe&, size_t flags) override;
};


class expr_sum_columnset : public expr_columnset {
  private:
    pexpr lhs;
    pexpr rhs;

  public:
    expr_sum_columnset(pexpr&& a, pexpr&& b);
    collist_ptr convert_to_collist(workframe&, size_t flags) override;
};


class expr_diff_columnset : public expr_columnset {
  private:
    pexpr lhs;
    pexpr rhs;

  public:
    expr_diff_columnset(pexpr&& a, pexpr&& b);
    collist_ptr convert_to_collist(workframe&, size_t flags) override;
};




}}
#endif
