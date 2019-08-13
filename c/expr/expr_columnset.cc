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


// void expr_columnset::add(size_t dfid, py::robj selector) {
//   columns_in.push_back({selector, dfid, true});
// }

// void expr_columnset::remove(size_t dfid, py::robj selector) {
//   columns_in.push_back({selector, dfid, false});
// }


bool expr_columnset::is_columnset_expr() const {
  return true;
}

SType expr_columnset::resolve(const workframe&) {
  throw RuntimeError() << "Method resolve() should not be called on expr_columnset";
}
GroupbyMode expr_columnset::get_groupby_mode(const workframe&) const {
  throw RuntimeError() << "Method get_groupby_mode() should not be called on expr_columnset";
}
OColumn expr_columnset::evaluate_eager(workframe&) {
  throw RuntimeError() << "Method evaluate_eager() should not be called on expr_columnset";
}


std::vector<SType> expr_columnset::resolve_all(const workframe& wf) {
  _resolve_out_columns(wf);
  std::vector<SType> res;
  for (const auto& col : columns_out) {
    res.push_back(col->resolve(wf));
  }
  return res;
}


std::vector<OColumn> expr_columnset::evaluate_eager_all(workframe& wf) {
  std::vector<OColumn> res;
  for (const auto& col : columns_out) {
    res.push_back(col->evaluate_eager(wf));
  }
  return res;
}


std::vector<vcolptr> expr_columnset::evaluate_lazy_all(workframe& wf) {
  std::vector<vcolptr> res;
  for (const auto& col : columns_out) {
    res.push_back(col->evaluate_lazy(wf));
  }
  return res;
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
  std::string colname = wf.get_datatable(frame_id)->get_names()[col_id];
  if (frame_id == 0) {
    // FIXME
    // return collist_ptr(new cols_intlist({col_id}, {std::move(colname)}));
  } else {
    // FIXME
    // return collist_ptr(new cols_exprlist({std::move(arg)},
                                         // {std::move(colname)}));
  }
    throw NotImplError();
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
  if (list1->is_simple_list() && list2->is_simple_list()) {
    // auto ilist1 = static_cast<cols_intlist*>(list1.get());
    // auto ilist2 = static_cast<cols_intlist*>(list2.get());
    // intvec iii;
    // strvec nnn;
    // iii.insert(iii.end(), ilist1->indices.begin(), ilist1->indices.end());
    // iii.insert(iii.end(), ilist2->indices.begin(), ilist2->indices.end());
    // nnn.insert(nnn.end(), ilist1->names.begin(), ilist1->names.end());
    // nnn.insert(nnn.end(), ilist2->names.begin(), ilist2->names.end());
    // return collist_ptr(new cols_intlist(std::move(iii), std::move(nnn)));
  }
  throw NotImplError();
}




//------------------------------------------------------------------------------
// expr_diff_columnset
//------------------------------------------------------------------------------

expr_diff_columnset::expr_diff_columnset(pexpr&& a, pexpr&& b)
  : lhs( _convert_to_columnset(std::move(a)) ),
    rhs( _convert_to_columnset(std::move(b)) ) {}


collist_ptr expr_diff_columnset::convert_to_collist(workframe& wf) {
  throw NotImplError();
}




//------------------------------------------------------------------------------
// Private helpers
//------------------------------------------------------------------------------

void expr_columnset::_resolve_out_columns(const workframe& wf) {
  for (const item_in& item : columns_in) {
    py::robj arg = item.selector;
    bool pos = item.positive;
    uint32_t frid = item.frame_id;
    if (arg.is_slice()) {
      _add_remove(wf, pos, _resolve_slice(wf, frid, arg));
    }
    else if (arg.is_list()) {
      _add_remove(wf, pos, _resolve_list(arg));
    }
    else if (arg.is_dtexpr()) {
      _add_remove(wf, pos, _resolve_single(arg));
    }
    else {
      throw TypeError() << "Unsupported expression of type " << arg.typeobj()
          << " when resolving a list of columns";
    }
  }
}


void expr_columnset::_add_remove(const workframe& wf, bool add, outvec&& items)
{
  if (add) {
    // outvec contains unique-ptrs which are moveable but not copyable.
    // Therefore, we use move-insert instead of a simple insert.
    columns_out.insert(
      columns_out.end(),
      std::make_move_iterator(items.begin()),
      std::make_move_iterator(items.end())
    );
  }
  else {  // Remove
    for (const pexpr& item : items) {
      if (item->is_column_expr()) {
        size_t col_index = item->get_col_index(wf);
        size_t col_frame = item->get_col_frame(wf);
        for (size_t i = 0; i < columns_out.size(); ++i) {
          if (col_frame == columns_out[i]->get_col_frame(wf) &&
              col_index == columns_out[i]->get_col_index(wf)) {
            columns_out.erase(columns_out.begin() + static_cast<long>(i));
            break;
          }
        }
        // If the item to be erased not found, then don't treat this as an
        // error.
      }
      else {
        throw TypeError() << "Column expression cannot be removed from a "
            "list of columns";
      }
    }
  }
}


std::vector<pexpr> expr_columnset::_resolve_slice(
    const workframe& wf, uint32_t frame_id, py::robj src)
{
  py::oslice ssrc = src.to_oslice();
  if (ssrc.is_numeric()) return _resolve_numslice(wf, frame_id, ssrc);
  if (ssrc.is_string())  return _resolve_strslice(wf, frame_id, ssrc);
  throw TypeError() << ssrc << " is neither integer- nor string-valued";
}


std::vector<pexpr> expr_columnset::_resolve_numslice(
    const workframe& wf, uint32_t frame_id, py::oslice ssrc)
{
  size_t len = wf.get_datatable(frame_id)->ncols;
  size_t start, count, step;
  ssrc.normalize(len, &start, &count, &step);
  outvec res;
  for (size_t i = 0; i < count; ++i) {
    res.emplace_back(new expr_column(frame_id, start + i * step));
  }
  return res;
}


std::vector<pexpr> expr_columnset::_resolve_strslice(
    const workframe& wf, uint32_t frame_id, py::oslice ssrc)
{
  outvec res;
  DataTable* dt0 = wf.get_datatable(frame_id);
  py::oobj ostart = ssrc.start_obj();
  py::oobj ostop = ssrc.stop_obj();
  size_t start = ostart.is_none()? 0 : dt0->xcolindex(ostart);
  size_t end = ostop.is_none()? dt0->ncols - 1 : dt0->xcolindex(ostop);
  size_t len = start <= end? end - start + 1 : start - end + 1;
  if (start <= end) {
    for (size_t i = start; i <= end; ++i) {
      res.emplace_back(new expr_column(frame_id, i));
    }
  } else {
    // Careful with the case when `end = 0`. In this case a regular for-loop
    // `(i = start; i >= end; --i)` becomes infinite.
    size_t i = start;
    do {
      res.emplace_back(new expr_column(frame_id, i));
    } while (i-- != end);
  }
  return res;
}


std::vector<pexpr> expr_columnset::_resolve_list(py::robj src) {
  outvec res;
  auto srclist = src.to_pylist();
  for (size_t i = 0; i < srclist.size(); ++i) {
    res.push_back(srclist[i].to_dtexpr());
  }
  return res;
}


std::vector<pexpr> expr_columnset::_resolve_single(py::robj src) {
  // return outvec({ src.to_dtexpr() });
  throw NotImplError();
}



}} // namespace dt::expr
