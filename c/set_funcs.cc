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
#include "datatablemodule.h"
#include "datatable.h"
#include "expr/py_expr.h"
#include "frame/py_frame.h"
#include "python/args.h"
#include "utils/exceptions.h"


//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------

namespace dt {
namespace set {

static py::PKArgs args_unique(
    1, 0, 0, false, false,
    {"frame"}, "unique",
R"(unique(frame)
--

Find the unique values in the ``frame``.

If the Frame has multiple columns, those columns must have the same logical
type (i.e. either `int` or `real` or `str`, etc). An error will be thrown if
the columns' ltypes are different.
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



} // namespace set
} // namespace dt


void DatatableModule::init_methods_sets() {
  ADDFN(dt::set::args_unique);
}
