//------------------------------------------------------------------------------
// Copyright 2018-2021 H2O.ai
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
#include <memory>              // std::unique_ptr
#include "frame/py_frame.h"    // py::Frame
#include "python/args.h"       // py::PKArgs
#include "sort/insert-sort.h"  // insert_sort
#include "sort/radix-sort.h"   // RadixSort
#include "sort/sorter.h"       // Sorter
#include "sort/sorter_bool.h"  // Sorter_Bool
#include "sort/sorter_float.h" // Sorter_Float
#include "sort/sorter_int.h"   // Sorter_Int
#include "sort/sorter_multi.h" // Sorter_Multi
#include "utils/assert.h"      // xassert
#include "column.h"            // Column
#include "rowindex.h"          // RowIndex
#include "stype.h"
namespace dt {
namespace sort {




template <typename T, bool ASC>
static std::unique_ptr<SSorter<T>> _make_sorter(const Column& col)
{
  using so = std::unique_ptr<SSorter<T>>;
  switch (col.stype()) {
    case SType::BOOL:    return make_sorter_bool<T, ASC>(col);
    case SType::INT8:    return so(new Sorter_Int<T, ASC, int8_t>(col));
    case SType::INT16:   return so(new Sorter_Int<T, ASC, int16_t>(col));
    case SType::DATE32:
    case SType::INT32:   return so(new Sorter_Int<T, ASC, int32_t>(col));
    case SType::TIME64:
    case SType::INT64:   return so(new Sorter_Int<T, ASC, int64_t>(col));
    case SType::FLOAT32: return so(new Sorter_Float<T, ASC, float>(col));
    case SType::FLOAT64: return so(new Sorter_Float<T, ASC, double>(col));
    default: throw TypeError() << "Cannot sort column of type " << col.stype();
  }
}

template <typename T>
static std::unique_ptr<SSorter<T>> _make_sorter(const colvec& cols) {
  using so = std::unique_ptr<SSorter<T>>;
  std::vector<so> sorters;
  sorters.reserve(cols.size());
  for (const Column& col : cols) {
    sorters.push_back(_make_sorter<T, true>(col));
  }
  return so(new Sorter_Multi<T>(std::move(sorters)));
}


using psorter = std::unique_ptr<Sorter>;

psorter make_sorter(const Column& col, Direction dir) {
  bool small = (col.nrows() <= dt::sort::MAX_NROWS_INT32);
  bool asc   = (dir == Direction::ASCENDING);
  return small? (asc? psorter(_make_sorter<int32_t, true>(col))
                    : psorter(_make_sorter<int32_t, false>(col)))
              : (asc? psorter(_make_sorter<int64_t, true>(col))
                    : psorter(_make_sorter<int64_t, false>(col)));
}

psorter make_sorter(const std::vector<Column>& cols) {
  xassert(cols.size() > 1);
  return (cols[0].nrows() <= dt::sort::MAX_NROWS_INT32)
      ? psorter(_make_sorter<int32_t>(cols))
      : psorter(_make_sorter<int64_t>(cols));
}



}}  // namespace dt::sort




namespace py {


static PKArgs args_newsort(0, 0, 0, false, false, {}, "newsort", nullptr);

oobj Frame::newsort(const PKArgs&) {
  xassert(dt->ncols() >= 1);
  xassert(dt->nrows() > 1);

  auto sorter = (dt->ncols() == 1)? dt::sort::make_sorter(dt->get_column(0), dt::sort::Direction::ASCENDING)
                                  : dt::sort::make_sorter(dt->get_columns());
  RiGb rigb = sorter->sort(dt->nrows(), false);
  Column ricol = rigb.first.as_column(dt->nrows());
  return py::Frame::oframe(new DataTable({std::move(ricol)}, {"order"}));
}


void Frame::_init_newsort(XTypeMaker& xt) {
  xt.add(METHOD(&Frame::newsort, args_newsort));
}


}
