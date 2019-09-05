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
#include <iterator>
#include "expr/expr_column.h"
#include "expr/expr_columnset.h"
#include "expr/workframe.h"
#include "utils/exceptions.h"
namespace dt {
namespace expr {



//------------------------------------------------------------------------------
// expr_columnset
//------------------------------------------------------------------------------

bool expr_columnset::is_columnset_expr() const {
  return true;
}


SType expr_columnset::resolve(const workframe&) {
  throw RuntimeError()
    << "Method resolve() should not be called on expr_columnset";
}

GroupbyMode expr_columnset::get_groupby_mode(const workframe&) const {
  throw RuntimeError()
    << "Method get_groupby_mode() should not be called on expr_columnset";
}

Column expr_columnset::evaluate(workframe&) {
  throw RuntimeError()
    << "Method evaluate() should not be called on expr_columnset";
}




//------------------------------------------------------------------------------
// expr_simple_columnset
//------------------------------------------------------------------------------

expr_simple_columnset::expr_simple_columnset(size_t frid, py::robj arg)
  : frame_id(frid), selector(arg) {}


collist_ptr expr_simple_columnset::convert_to_collist(
    workframe& wf, size_t flags)
{
  return collist_ptr(new collist(wf, selector, flags, frame_id));
}




//------------------------------------------------------------------------------
// expr_singlecol_columnset
//------------------------------------------------------------------------------

expr_singlecol_columnset::expr_singlecol_columnset(pexpr&& a)
  : arg(std::move(a)) {}


collist_ptr expr_singlecol_columnset::convert_to_collist(
    workframe& wf, size_t flags)
{
  bool newcol_ok = (flags & collist::ALLOW_NEW_COLUMNS);
  auto colexpr = dynamic_cast<expr_column*>(arg.get());
  if (colexpr) {
    size_t frame_id = colexpr->get_col_frame(wf);
    size_t col_id = colexpr->get_col_index(wf, /*strict=*/ !newcol_ok);
    if (frame_id == 0) {
      return collist_ptr(new collist({}, {col_id}, {}));
    } else {
      std::string colname = (col_id == size_t(-1))
                            ? ""
                            : wf.get_datatable(frame_id)->get_names()[col_id];
      exprvec exprs;
      exprs.push_back(std::move(arg));
      return collist_ptr(new collist(std::move(exprs), {},
                                     {std::move(colname)}));
    }
  } else {
    exprvec exprs;
    exprs.push_back(std::move(arg));
    return collist_ptr(new collist(std::move(exprs), {}, {}));
  }
}




//------------------------------------------------------------------------------
// expr_sum_columnset
//------------------------------------------------------------------------------

static pexpr _convert_to_columnset(pexpr&& expr) {
  if (expr->is_columnset_expr()) {
    return std::move(expr);
  }
  if (expr->is_literal_expr()) {
    return pexpr(new expr_simple_columnset(0, expr->get_literal_arg()));
  }
  return pexpr(new expr_singlecol_columnset(std::move(expr)));
}


expr_sum_columnset::expr_sum_columnset(pexpr&& a, pexpr&& b)
  : lhs( _convert_to_columnset(std::move(a)) ),
    rhs( _convert_to_columnset(std::move(b)) ) {}


collist_ptr expr_sum_columnset::convert_to_collist(
    workframe& wf, size_t flags)
{
  auto cs1 = static_cast<expr_columnset*>(lhs.get());
  auto cs2 = static_cast<expr_columnset*>(rhs.get());
  auto list1 = cs1->convert_to_collist(wf, flags);
  auto list2 = cs2->convert_to_collist(wf, flags);
  list1->append(std::move(list2));
  return list1;
}




//------------------------------------------------------------------------------
// expr_diff_columnset
//------------------------------------------------------------------------------

expr_diff_columnset::expr_diff_columnset(pexpr&& a, pexpr&& b)
  : lhs( _convert_to_columnset(std::move(a)) ),
    rhs( _convert_to_columnset(std::move(b)) ) {}


collist_ptr expr_diff_columnset::convert_to_collist(
    workframe& wf, size_t flags)
{
  auto cs1 = static_cast<expr_columnset*>(lhs.get());
  auto cs2 = static_cast<expr_columnset*>(rhs.get());
  auto list1 = cs1->convert_to_collist(wf, flags);
  auto list2 = cs2->convert_to_collist(wf, flags | collist::ALLOW_NEW_COLUMNS);
  list1->exclude(std::move(list2));
  return list1;
}




}} // namespace dt::expr
