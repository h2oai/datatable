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
#include "utils/exceptions.h"
namespace dt {
namespace expr {


template <typename T>
static void concat_vectors(std::vector<T>& a, std::vector<T>&& b) {
  a.reserve(a.size() + b.size());
  for (size_t i = 0; i < b.size(); ++i) {
    a.push_back(std::move(b[i]));
  }
}

template <typename T>
static void delete_vector_element(std::vector<T>& vec, size_t index) {
  if (index < vec.size()) {
    vec.erase(vec.begin() + static_cast<long>(index));
  }
}




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

OColumn expr_columnset::evaluate_eager(workframe&) {
  throw RuntimeError()
    << "Method evaluate_eager() should not be called on expr_columnset";
}




//------------------------------------------------------------------------------
// expr_simple_columnset
//------------------------------------------------------------------------------

expr_simple_columnset::expr_simple_columnset(size_t frid, py::robj arg)
  : frame_id(frid), selector(arg) {}


collist_ptr expr_simple_columnset::convert_to_collist(workframe& wf) {
  return collist_ptr(new collist(wf, selector, "", frame_id));
}




//------------------------------------------------------------------------------
// expr_singlecol_columnset
//------------------------------------------------------------------------------

expr_singlecol_columnset::expr_singlecol_columnset(pexpr&& a)
  : arg(std::move(a))
{
  xassert(arg->is_column_expr());
}


collist_ptr expr_singlecol_columnset::convert_to_collist(workframe& wf) {
  size_t frame_id = arg->get_col_frame(wf);
  size_t col_id = arg->get_col_index(wf);
  if (frame_id == 0) {
    return collist_ptr(new collist({}, {col_id}, {}));
  } else {
    std::string colname = wf.get_datatable(frame_id)->get_names()[col_id];
    exprvec exprs;
    exprs.push_back(std::move(arg));
    return collist_ptr(new collist(std::move(exprs), {},
                                   {std::move(colname)}));
  }
}




//------------------------------------------------------------------------------
// expr_sum_columnset
//------------------------------------------------------------------------------

static pexpr _convert_to_columnset(pexpr&& expr) {
  if (expr->is_columnset_expr()) {
    return std::move(expr);
  }
  if (expr->is_column_expr()) {
    return pexpr(new expr_singlecol_columnset(std::move(expr)));
  }
  if (expr->is_literal_expr()) {
    return pexpr(new expr_simple_columnset(0, expr->get_literal_arg()));
  }
  throw TypeError() << "Cannot convert object into a columnset";
}


expr_sum_columnset::expr_sum_columnset(pexpr&& a, pexpr&& b)
  : lhs( _convert_to_columnset(std::move(a)) ),
    rhs( _convert_to_columnset(std::move(b)) ) {}


collist_ptr expr_sum_columnset::convert_to_collist(workframe& wf) {
  auto list1 = static_cast<expr_columnset*>(lhs.get())->convert_to_collist(wf);
  auto list2 = static_cast<expr_columnset*>(rhs.get())->convert_to_collist(wf);
  auto names1   = list1->release_names();
  auto names2   = list2->release_names();
  auto indices1 = list1->release_indices();
  auto indices2 = list2->release_indices();
  auto exprs1   = list1->release_exprs();
  auto exprs2   = list2->release_exprs();
  concat_vectors(names1, std::move(names2));
  concat_vectors(exprs1, std::move(exprs2));
  concat_vectors(indices1, std::move(indices2));
  if (indices1.size() < exprs1.size()) indices1.clear();
  return collist_ptr(new collist(std::move(exprs1),
                                 std::move(indices1),
                                 std::move(names1)));
}




//------------------------------------------------------------------------------
// expr_diff_columnset
//------------------------------------------------------------------------------

expr_diff_columnset::expr_diff_columnset(pexpr&& a, pexpr&& b)
  : lhs( _convert_to_columnset(std::move(a)) ),
    rhs( _convert_to_columnset(std::move(b)) ) {}


collist_ptr expr_diff_columnset::convert_to_collist(workframe& wf) {
  auto list1 = static_cast<expr_columnset*>(lhs.get())->convert_to_collist(wf);
  auto list2 = static_cast<expr_columnset*>(rhs.get())->convert_to_collist(wf);
  if (!list2->is_simple_list()) {
    throw TypeError() << "Column expression cannot be removed from a columnset";
  }
  auto exprs1   = list1->release_exprs();
  auto names1   = list1->release_names();
  auto indices1 = list1->release_indices();
  auto indices2 = list2->release_indices();
  for (size_t i = 0; i < indices2.size(); ++i) {
    for (size_t j = 0; j < indices1.size(); ++j) {
      if (indices1[j] == indices2[i]) {
        delete_vector_element(exprs1, j);
        delete_vector_element(names1, j);
        delete_vector_element(indices1, j);
        break;
      }
    }
    // If the item to be erased not found, then don't treat this as an error.
  }
  return collist_ptr(new collist(std::move(exprs1),
                                 std::move(indices1),
                                 std::move(names1)));
}




}} // namespace dt::expr
