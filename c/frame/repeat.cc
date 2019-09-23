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
#include "frame/py_frame.h"
#include "python/args.h"
#include "rowindex.h"
#include "column_impl.h"  // TODO: remove


//------------------------------------------------------------------------------
// Static helpers
//------------------------------------------------------------------------------

// TODO: we could create a special "repeated" column here
Column ColumnImpl::repeat(size_t nreps) const {
  xassert(!info(_stype).is_varwidth());
  size_t esize = info(_stype).elemsize();
  size_t new_nrows = _nrows * nreps;

  Column newcol = Column::new_data_column(_stype, new_nrows);
  if (!new_nrows) {
    return newcol;
  }
  const void* olddata = data();
  void* newdata = newcol->data_w();

  std::memcpy(newdata, olddata, _nrows * esize);
  size_t nrows_filled = _nrows;
  while (nrows_filled < new_nrows) {
    size_t nrows_copy = std::min(new_nrows - nrows_filled, nrows_filled);
    std::memcpy(static_cast<char*>(newdata) + nrows_filled * esize,
                newdata,
                nrows_copy * esize);
    nrows_filled += nrows_copy;
    xassert(nrows_filled % _nrows == 0);
  }
  xassert(nrows_filled == new_nrows);

  return newcol;
}



//------------------------------------------------------------------------------
// datatable.repeat()
//------------------------------------------------------------------------------
namespace py {

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
  if (dt->ncols == 0 || dt->nrows == 0) {
    return Frame::oframe(dt->copy());
  }

  colvec newcols(dt->ncols);
  for (size_t i = 0; i < dt->ncols; ++i) {
    newcols[i] = dt->get_column(i);  // copy
    newcols[i].repeat(n);
  }
  DataTable* newdt = new DataTable(std::move(newcols),
                                   dt);  // copy names from dt
  return Frame::oframe(newdt);
}



void DatatableModule::init_methods_repeat() {
  ADD_FN(&repeat, args_repeat);
}

} // namespace py
