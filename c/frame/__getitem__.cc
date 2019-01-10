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
// This is a main file dedicated to computing the pythonic expression
//
//     DT[i, j, by(), join(), ...]
//
// Here `i` can be either an int, a slice, a range, a list of ints, a generator
// expression, a boolean or integer Frame, a numpy array, etc. We convert all
// of these various forms into an `i_node` object.
//
// Similarly, the `j` item can also be an int, a string, a slice, a range,
// an expression, a type, an stype, a list/tuple/iterator of any of these, etc.
// We build a `j_node` object out of the `j` expression.
//
// On the contrary, the python `by()` function already creates a "by_node"
// object, so there is no need to instantiate anything. The only thing remaining
// is to verify the correctness of columns / column expressions used in this
// `by_node`.
//
// Likewise, each `join()` call in python creates a "join_node" object, and we
// only need to collect these objects into the workframe.
//
//
// The "workframe" object gathers all the information necessary to evaluate the
// expression `DT[i, j, ...]` above.
//
// We begin evaluation with checking the by- and join- nodes and inserting them
// into the workframe. This has to be done first, because `i` and `j` nodes can
// refer to columns from the join frames, and therefore in order to check their
// correctness we need to know which frames are joined.
//
// Next, we construct `i_node` and `j_node` from arguments `i` and `j`.
//
// Once all nodes of the evaluation graph are initialized, we compute all joins
// (if any). After this step all subframes within the evaluation frame will
// become conformant, i.e. they'll have the same number of rows (after applying
// each subframe's RowIndex).
//
//------------------------------------------------------------------------------
#include "expr/base_expr.h"
#include "expr/by_node.h"
#include "expr/i_node.h"
#include "expr/j_node.h"
#include "expr/join_node.h"
#include "expr/workframe.h"
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


oobj Frame::_main_getset(robj item, robj value) {
  rtuple targs = item.to_rtuple_lax();
  size_t nargs = targs? targs.size() : 0;
  if (nargs <= 1) {
    throw ValueError() << "Single-item selectors `DT[col]` are prohibited "
        "since 0.8.0; please use `DT[:, col]`. In 0.9.0 this expression "
        "will be interpreted as a row selector instead.";
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
      int64_t nrows = static_cast<int64_t>(dt->nrows);
      int64_t ncols = static_cast<int64_t>(dt->ncols);
      if (irow < -nrows || irow >= nrows) {
        throw ValueError() << "Row `" << irow << "` is invalid for a frame "
            "with " << nrows << " row" << (nrows == 1? "" : "s");
      }
      if (irow < 0) irow += nrows;
      size_t zrow = static_cast<size_t>(irow);
      size_t zcol;
      if (a1int) {
        int64_t icol = arg1.to_int64_strict();
        if (icol < -ncols || icol >= ncols) {
          throw ValueError() << "Column index `" << icol << "` is invalid "
              "for a frame with " << ncols << " column" <<
              (ncols == 1? "" : "s");
        }
        if (icol < 0) icol += ncols;
        zcol = static_cast<size_t>(icol);
      } else {
        zcol = dt->xcolindex(arg1);
      }
      Column* col = dt->columns[zcol];
      return col->get_value_at_index(zrow);
    }
    // otherwise fall-through
  }

  // 1. Create the workframe
  dt::workframe wf(dt);
  wf.set_mode(value == GETITEM? dt::EvalMode::SELECT :
              value == DELITEM? dt::EvalMode::DELETE :
                                dt::EvalMode::UPDATE);

  // 2. Search for join nodes in order to bind all aliases and
  //    to know which frames participate in `DT[...]`.
  for (size_t k = 2; k < nargs; ++k) {
    robj arg = targs[k];
    auto arg_join = arg.to_ojoin_lax();
    if (arg_join) {
      wf.add_join(arg_join);
      continue;
    }
    auto arg_by = arg.to_oby_lax();
    if (arg_by) {
      wf.add_groupby(arg_by);
      continue;
    }
    if (arg.is_none()) continue;
    if (k == 2 && (arg.is_string() || is_PyBaseExpr(arg))) {
      oby byexpr = oby::make(arg);
      wf.add_groupby(byexpr);
      continue;
    }
    throw TypeError() << "Invalid item at position " << k
        << " in DT[i, j, ...] call";
  }

  // 3. Instantiate `i_node` and `j_node`.
  auto iexpr = dt::i_node::make(targs[0], wf);
  auto jexpr = dt::j_node::make(targs[1], wf);
  xassert(iexpr && jexpr);

  // 4. Perform joins.
  wf.compute_joins();

  if (!wf.has_groupby()) {
    iexpr->execute(wf);
    if (value == GETITEM) {
      DataTable* res = jexpr->select(wf);
      return oobj::from_new_reference(py::Frame::from_datatable(res));
    }
    if (value == DELITEM) {
      jexpr->delete_(wf);
      _clear_types();
      return py::None();
    }
  }

  return _fallback_getset(item, value);
}


oobj Frame::_fallback_getset(robj item, robj value) {
  odict kwargs;
  otuple args(5);
  args.set(0, py::robj(this));
  if (item.is_tuple()) {
    otuple argslist = item.to_otuple();
    size_t n = argslist.size();
    if (n >= 2) {
      args.set(1, argslist[0]);
      args.set(2, argslist[1]);
      if (n == 3) {
        args.set(3, argslist[2]);
      } else if (n >= 4) {
        throw ValueError() << "Selector " << item << " is not supported";
      }
    } else {
      throw ValueError() << "Invalid selector " << item;
    }
  }
  if (!args[3]) args.set(3, py::None());
  if (!args[4]) args.set(4, py::None());
  if (value == GETITEM) {}
  else if (value) {
    kwargs.set(ostring("mode"), ostring("update"));
    kwargs.set(ostring("replacement"), value);
  } else {
    kwargs.set(ostring("mode"), ostring("delete"));
  }
  return robj(py::fallback_makedatatable).call(args, kwargs);
}


}  // namespace py
