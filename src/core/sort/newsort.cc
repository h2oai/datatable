//------------------------------------------------------------------------------
// Copyright 2018-2020 H2O.ai
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
#include "frame/py_frame.h"    // py::Frame
#include "python/args.h"       // py::PKArgs
#include "sort/insert-sort.h"  // insert_sort
#include "sort/radix-sort.h"   // RadixSort
#include "sort/sorter_bool.h"  // Sorter_Bool
#include "sort/sorter_int.h"   // Sorter_Int
#include "utils/assert.h"      // xassert
#include "column.h"            // Column
#include "rowindex.h"          // RowIndex
namespace dt {
namespace sort {





template <typename TO>
static std::unique_ptr<Sorter<TO>> _make_sorter(const Column& col) {
  using so = std::unique_ptr<Sorter<TO>>;
  switch (col.stype()) {
    case SType::BOOL:  return so(new Sorter_Bool<TO>(col));
    case SType::INT8:  return so(new Sorter_Int<TO, int8_t>(col));
    case SType::INT16: return so(new Sorter_Int<TO, int16_t>(col));
    case SType::INT32: return so(new Sorter_Int<TO, int32_t>(col));
    case SType::INT64: return so(new Sorter_Int<TO, int64_t>(col));
    default: throw TypeError() << "Cannot sort column of type " << col.stype();
  }
}



}}  // namespace dt::sort




namespace py {


static PKArgs args_newsort(0, 0, 0, false, false, {}, "newsort", nullptr);

oobj Frame::newsort(const PKArgs&) {
  xassert(dt->ncols() >= 1);
  xassert(dt->nrows() > 1);

  const Column& col0 = dt->get_column(0);
  size_t n = dt->nrows();
  RowIndex rowindex;
  if (n <= dt::sort::MAX_NROWS_INT32) {
    auto sorter = dt::sort::_make_sorter<int32_t>(col0);
    rowindex = sorter->sort();
  } else {
    auto sorter = dt::sort::_make_sorter<int64_t>(col0);
    rowindex = sorter->sort();
  }
  Column ricol = rowindex.as_column(n);
  return py::Frame::oframe(new DataTable({std::move(ricol)}, {"order"}));
}


void Frame::_init_newsort(XTypeMaker& xt) {
  xt.add(METHOD(&Frame::newsort, args_newsort));
}


}
