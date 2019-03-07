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
#include "frame/py_frame.h"
#include "python/_all.h"
#include "python/arg.h"
#include "utils/exceptions.h"
#include "column.h"
#include "datatable.h"
#include "datatablemodule.h"

namespace py {


//------------------------------------------------------------------------------
// kfold()
//------------------------------------------------------------------------------

static PKArgs args_kfold_simple(
  0, 0, 2, false, false,
  {"nrows", "nsplits"}, "kfold",

R"(kfold(nrows, nsplits)
--

Perform k-fold split of data with `nrows` rows into `nsplits` train/test
subsets.

This function will return a list of `nsplits` tuples `(train_rows, test_rows)`,
where each component of the tuple is a rows selector that can be applied to a
frame with `nrows` rows. Some of these row selectors will be simple python
ranges, others will be single-column Frame objects.

The range `[0; nrows)` is split into `nsplits` approximately equal parts
(called "folds"), and then each `i`-th split will use the `i`-th fold as a
test part, and all the remaining rows as the train part. Thus, `i`-th split is
comprised of:

  - train: rows [0; i*nrows/nsplits) + [(i+1)*nrows/nsplits; nrows)
  - test:  rows [i*nrows/nsplits; (i+1)*nrows/nsplits)

where integer division is assumed.

Parameters
----------
nrows: int
    The number of rows in the frame that you want to split.

nsplits: int
    Number of folds, must be at least 2, but not larger than `nrows`.
)");


static oobj kfold(const PKArgs& args) {
  if (!args[0]) throw TypeError() << "Required parameter `nrows` is missing";
  if (!args[1]) throw TypeError() << "Required parameter `nsplits` is missing";
  size_t nrows = args[0].to_size_t();
  size_t nsplits = args[1].to_size_t();
  if (nsplits < 2) {
    throw ValueError() << "The number of splits cannot be less than two";
  }
  if (nsplits > nrows) {
    throw ValueError()
      << "The number of splits cannot exceed the number of rows";
  }

  int64_t k = static_cast<int64_t>(nsplits);
  int64_t n = static_cast<int64_t>(nrows);

  olist res(nsplits);
  res.set(0, otuple(orange(n/k, n),
                    orange(0, n/k)));
  res.set(nsplits-1, otuple(orange(0, (k-1)*n/k),
                            orange((k-1)*n/k, n)));

  std::vector<int32_t*> data;

  for (int64_t ii = 1; ii < k-1; ++ii) {
    int64_t b1 = ii*n/k;
    int64_t b2 = (ii+1)*n/k;
    size_t colsize = static_cast<size_t>(b1 + n - b2);
    Column* col = Column::new_data_column(SType::INT32, colsize);
    DataTable* dt = new DataTable({col});
    data.push_back(static_cast<int32_t*>(col->data_w()));

    res.set(static_cast<size_t>(ii),
            otuple(oobj::from_new_reference(Frame::from_datatable(dt)),
                   orange(b1, b2)));
  }

  for (int64_t t = 0; t < k*(k-2); ++t) {
    int64_t j = t / k;     // column index
    int64_t i = t - j * k; // block of rows
    ++j;
    if (i == j) continue;
    int64_t row0 = i * n / k;
    int64_t row1 = (i+1) * n / k;
    int64_t delta = i < j? 0 : (j+1)*n/k - j*n/k;
    int32_t* ptr = data[static_cast<size_t>(j) - 1] - delta;
    for (int64_t row = row0; row < row1; ++row) {
      ptr[row] = static_cast<int32_t>(row);
    }
  }

  return std::move(res);
}



void DatatableModule::init_methods_kfold() {
  ADD_FN(&kfold, args_kfold_simple);
}


} // namespace py
