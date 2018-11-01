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
#include <tuple>
#include "datatablemodule.h"
#include "datatable.h"
#include "expr/py_expr.h"
#include "frame/py_frame.h"
#include "python/args.h"
#include "python/oiter.h"
#include "utils/assert.h"
#include "utils/exceptions.h"

namespace dt {
namespace set {

using ccolvec = std::vector<const Column*>;

struct sort_result {
  Column* col = nullptr;
  RowIndex ri;
  Groupby gb;
};


//------------------------------------------------------------------------------
// helper functions
//------------------------------------------------------------------------------

static void verify_frame_1column(DataTable* dt) {
  if (dt->ncols == 1) return;
  throw ValueError() << "Only single-column Frames are allowed, but received "
      "a Frame with " << dt->ncols << " columns";
}

static ccolvec columns_from_args(const py::PKArgs& args) {
  ccolvec res;
  for (auto va : args.varargs()) {
    if (va.is_frame()) {
      DataTable* dt = va.to_frame();
      if (dt->ncols == 0) continue;
      verify_frame_1column(dt);
      res.push_back(dt->columns[0]);
    }
    else if (va.is_iterable()) {
      for (auto item : va.to_pyiter()) {
        DataTable* dt = item.to_frame();
        if (dt->ncols == 0) continue;
        verify_frame_1column(dt);
        res.push_back(dt->columns[0]);
      }
    }
    else {
      throw TypeError() << args.get_short_name() << "() expected a list or "
          "sequence of Frames, but got an argument of type " << va.typeobj();
    }
  }
  return res;
}

static sort_result sort_columns(ccolvec& cols) {
  xassert(!cols.empty());
  sort_result res;
  if (cols.size() == 1) {
    res.col = cols[0]->shallowcopy();
    res.col->reify();
  } else {
    res.col = (new VoidColumn(0))->rbind(cols);
  }
  res.ri = res.col->sort(&res.gb);
  return res;
}



//------------------------------------------------------------------------------
// unique()
//------------------------------------------------------------------------------

static py::PKArgs fn_unique(
    1, 0, 0,        // Number of pos-only, pos/kw, and kw-only args
    false, false,   // varargs/varkws allowed?
    {"frame"},      // arg names
    "unique",       // function name
R"(unique(frame)
--

Find the unique values in the ``frame``.

If the Frame has multiple columns, those columns must have the same logical
type (i.e. either `int` or `real` or `str`, etc). An error will be thrown if
the columns' ltypes are different.

This methods sorts the values in order to find the uniques. Thus, the return
values will be sorted. However, this should be considered an implementation
detail: in the future we may use a different algorithm (such as hash-based),
which may return the results in different order.
)",

[](const py::PKArgs& args) -> py::oobj {
  DataTable* dt = args[0].to_frame();

  if (dt->ncols == 1) {
    Groupby gb;
    RowIndex ri = dt->columns[0]->sort(&gb);
    Column* colri = dt->columns[0]->shallowcopy(ri);
    Column* uniques = expr::reduce_first(colri, gb);
    DataTable* res = new DataTable({uniques}, dt);
    return py::oobj::from_new_reference(py::Frame::from_datatable(res));
  }

  throw NotImplError();
});



//------------------------------------------------------------------------------
// union()
//------------------------------------------------------------------------------

static py::PKArgs fn_union(
    0, 0, 0,
    true, false,
    {},
    "union",
R"(union(*frames)
--

Find set union of values in all `frames`.

Each frame must be a single-column, and have the same logical type (i.e. either
`int` or `real` or `str`). This function will treat each column as a set,
perform Union operation on these sets, and return the result of this union as a
single-column Frame.

This operation is equivalent to ``dt.unique(dt.rbind(*frames))``.
)",

[](const py::PKArgs& args) -> py::oobj {
  ccolvec cols = columns_from_args(args);
  sort_result s = sort_columns(cols);
  throw NotImplError();
});





} // namespace set
} // namespace dt


void DatatableModule::init_methods_sets() {
  ADDFN(dt::set::fn_unique);
  ADDFN(dt::set::fn_union);
}
