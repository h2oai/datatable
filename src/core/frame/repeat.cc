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
#include "column/repeated.h"
#include "frame/py_frame.h"
#include "python/args.h"
#include "datatablemodule.h"
#include "rowindex.h"
namespace py {



//------------------------------------------------------------------------------
// datatable.repeat()
//------------------------------------------------------------------------------

static PKArgs args_repeat(
    2, 0, 0, false, false, {"frame", "n"},
    "repeat",
R"(repeat(frame, n)
--

Concatenate `n` copies of the `frame` by rows and return the result.

This is equivalent to ``dt.rbind([self] * n)``.
)");


static oobj repeat(const PKArgs& args) {
  DataTable* dt = args[0].to_datatable();
  size_t n = args[1].to_size_t();

  // Empty Frame: repeating is a noop
  if (dt->ncols() == 0 || dt->nrows() == 0) {
    return Frame::oframe(new DataTable(*dt));
  }

  colvec newcols(dt->ncols());
  for (size_t i = 0; i < dt->ncols(); ++i) {
    newcols[i] = dt->get_column(i);  // copy
    newcols[i].repeat(n);
  }
  DataTable* newdt = new DataTable(std::move(newcols),
                                   *dt);  // copy names from dt
  return Frame::oframe(newdt);
}




void DatatableModule::init_methods_repeat() {
  ADD_FN(&repeat, args_repeat);
}



} // namespace py
