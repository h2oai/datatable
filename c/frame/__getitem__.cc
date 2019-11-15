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
//
// This file implements `Frame.__getitem__()` and `Frame.__setitem__()` methods,
// which are used for the []-functionality:
//
//     DT[i, j, by(), join(), ...]
//
// In this file we mainly gather all the python arguments into an `EvalContext`
// object, and then allow that object to calculate the result.
//
//------------------------------------------------------------------------------
#include "expr/eval_context.h"
#include "expr/expr.h"
#include "expr/py_by.h"         // py::oby
#include "expr/py_join.h"       // py::ojoin
#include "expr/py_sort.h"       // py::osort
#include "frame/py_frame.h"
#include "python/_all.h"
#include "python/string.h"
namespace py {

// Sentinel values for __getitem__() mode
static robj GETITEM(reinterpret_cast<PyObject*>(-1));
static robj DELITEM(reinterpret_cast<PyObject*>(0));



oobj Frame::m__getitem__(robj item) {
  return _main_getset(item, GETITEM);
}

void Frame::m__setitem__(robj item, robj value) {
  _main_getset(item, value);
}


//------------------------------------------------------------------------------
// Implementation of various selectors
//------------------------------------------------------------------------------

oobj Frame::_get_single_column(robj selector) {
  if (selector.is_int()) {
    size_t col_index = dt->xcolindex(selector.to_int64_strict());
    return Frame::oframe(dt->extract_column(col_index));
  }
  if (selector.is_string()) {
    size_t col_index = dt->xcolindex(selector);
    return Frame::oframe(dt->extract_column(col_index));
  }
  throw TypeError() << "Column selector must be an integer or a string, not "
                    << selector.typeobj();
}

oobj Frame::_del_single_column(robj selector) {
  if (selector.is_int()) {
    size_t col_index = dt->xcolindex(selector.to_int64_strict());
    intvec columns_to_delete = {col_index};
    dt->delete_columns(columns_to_delete);
  }
  else if (selector.is_string()) {
    size_t col_index = dt->xcolindex(selector);
    intvec columns_to_delete = {col_index};
    dt->delete_columns(columns_to_delete);
  }
  else {
    throw TypeError() << "Column selector must be an integer or a string, not "
                  << selector.typeobj();
  }
  _clear_types();
  return oobj();
}


oobj Frame::_main_getset(robj item, robj value) {
  rtuple targs = item.to_rtuple_lax();

  // Single-column-selector case. Commonly used expressions such as
  // DT[3] or DT["col"] will result in `item` being an int/string not
  // a tuple, and thus `targs` will be empty.
  if (!targs) {
    if (value == GETITEM) return _get_single_column(item);
    if (value == DELITEM) return _del_single_column(item);
    return _main_getset(otuple({py::None(), item}), value);
  }

  size_t nargs = targs.size();
  if (nargs <= 1) {
    // Selectors of the type `DT[tuple()]` or `DT[0,]`
    throw ValueError() << "Invalid tuple of size " << nargs
        << " used as a frame selector";
  }

  // "Fast" get/set only handles the case of the form `DT[i, j]` where
  // `i` is integer, and `j` is either an integer or a string. These cases
  // are special in that they return a scalar python value, instead of a
  // Frame object.
  // This case should also be handled first, to ensure that it has maximum
  // performance.
  if (nargs == 2 && value == GETITEM) {  // TODO: handle SETITEM too
    robj arg0 = targs[0], arg1 = targs[1];
    bool a0int = arg0.is_int();
    bool a1int = arg1.is_int();
    if (a0int && (a1int || arg1.is_string())) {
      int64_t irow = arg0.to_int64_strict();
      int64_t nrows = static_cast<int64_t>(dt->nrows());
      if (irow < -nrows || irow >= nrows) {
        throw ValueError() << "Row `" << irow << "` is invalid for a frame "
            "with " << nrows << " row" << (nrows == 1? "" : "s");
      }
      if (irow < 0) irow += nrows;
      size_t zrow = static_cast<size_t>(irow);
      size_t zcol = a1int? dt->xcolindex(arg1.to_int64_strict())
                         : dt->xcolindex(arg1);
      const Column& col = dt->get_column(zcol);
      return col.get_element_as_pyobject(zrow);
    }
    // otherwise fall-through
  }

  // 1. Create the EvalContext
  auto mode = value == GETITEM? dt::expr::EvalMode::SELECT :
              value == DELITEM? dt::expr::EvalMode::DELETE :
                                dt::expr::EvalMode::UPDATE;
  dt::expr::EvalContext ctx(dt, mode);

  // 2. Search for join nodes in order to bind all aliases and
  //    to know which frames participate in `DT[...]`.
  py::oby arg_by;
  py::ojoin arg_join;
  py::osort arg_sort;
  for (size_t k = 2; k < nargs; ++k) {
    robj arg = targs[k];
    if ((arg_join = arg.to_ojoin_lax())) {
      ctx.add_join(arg_join);
      continue;
    }
    if ((arg_by = arg.to_oby_lax())) {
      ctx.add_groupby(arg_by);
      continue;
    }
    if ((arg_sort = arg.to_osort_lax())) {
      ctx.add_sortby(arg_sort);
      continue;
    }
    if (arg.is_none()) continue;
    if (k == 2 && (arg.is_string() || arg.is_dtexpr())) {
      oby byexpr = oby::make(arg);
      ctx.add_groupby(byexpr);
      continue;
    }
    throw TypeError() << "Invalid item at position " << k
        << " in DT[i, j, ...] call";
  }

  // 3. Instantiate `i` and `j` nodes.
  xassert(nargs >= 2);
  ctx.add_i(targs[0]);
  ctx.add_j(targs[1]);

  if (mode == dt::expr::EvalMode::UPDATE) {
    ctx.add_replace(value);
  }

  auto res = ctx.evaluate();
  if (ctx.get_mode() != dt::expr::EvalMode::SELECT) {
    _clear_types();
  }
  return res;
}


}  // namespace py
