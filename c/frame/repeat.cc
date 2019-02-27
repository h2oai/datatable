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


//------------------------------------------------------------------------------
// Static helpers
//------------------------------------------------------------------------------

/**
 * Create and return a RowIndex with elements
 *
 *     list(range(nrows)) * nreps
 *
 * The returned RowIndex will be either ARR32 or ARR64 depending on how many
 * elements are in it.
 */
template <typename T>
static RowIndex _make_repeat_rowindex(size_t nrows, size_t nreps) {
  size_t new_nrows = nrows * nreps;
  dt::array<T> indices(new_nrows);
  T* ptr = indices.data();
  for (size_t i = 0; i < nrows; ++i) {
    ptr[i] = static_cast<T>(i);
  }
  size_t nrows_filled = nrows;
  while (nrows_filled < new_nrows) {
    size_t nrows_copy = std::min(new_nrows - nrows_filled, nrows_filled);
    std::memcpy(ptr + nrows_filled, ptr, nrows_copy * sizeof(T));
    nrows_filled += nrows_copy;
    xassert(nrows_filled % nrows == 0);
  }
  xassert(nrows_filled == new_nrows);
  return RowIndex(std::move(indices), 0, nrows - 1);
}


Column* Column::repeat(size_t nreps) {
  xassert(is_fixedwidth());
  xassert(!ri);
  size_t esize = elemsize();
  size_t new_nrows = nrows * nreps;

  Column* newcol = Column::new_data_column(stype(), new_nrows);
  if (!new_nrows) {
    return newcol;
  }
  const void* olddata = data();
  void* newdata = newcol->data_w();

  std::memcpy(newdata, olddata, nrows * esize);
  size_t nrows_filled = nrows;
  while (nrows_filled < new_nrows) {
    size_t nrows_copy = std::min(new_nrows - nrows_filled, nrows_filled);
    std::memcpy(static_cast<char*>(newdata) + nrows_filled * esize,
                newdata,
                nrows_copy * esize);
    nrows_filled += nrows_copy;
    xassert(nrows_filled % nrows == 0);
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
  DataTable* dt = args[0].to_frame();
  size_t n = args[1].to_size_t();

  // Empty Frame: repeating is a noop
  if (dt->ncols == 0 || dt->nrows == 0) {
    return oobj::from_new_reference(Frame::from_datatable(dt->copy()));
  }

  // Single-colum fixed-width Frame:
  Column* col0 = dt->columns[0];
  if (dt->ncols == 1 &&
      !info(col0->stype()).is_varwidth() &&
      !col0->rowindex())
  {
    Column* newcol = col0->repeat(n);
    DataTable* newdt = new DataTable({newcol}, dt);  // copy names from dt
    return oobj::from_new_reference(Frame::from_datatable(newdt));
  }

  constexpr size_t MAX32 = std::numeric_limits<int32_t>::max();
  size_t nn = dt->nrows * n;
  RowIndex ri = nn == n    ? RowIndex(size_t(0), n, 0) :
                nn < MAX32 ? _make_repeat_rowindex<int32_t>(dt->nrows, n)
                           : _make_repeat_rowindex<int64_t>(dt->nrows, n);

  DataTable* newdt = apply_rowindex(dt, ri);
  return oobj::from_new_reference(Frame::from_datatable(newdt));
}



void DatatableModule::init_methods_repeat() {
  ADD_FN(&repeat, args_repeat);
}

} // namespace py
