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

static PKArgs fn_kfold_simple(
  0, 2, 0, false, false,
  {"k", "n"}, "kfold",

R"(kfold(k, n)
--

Perform k-fold split of data with `n` rows into `k` train / test subsets.

This function will return a list of `k` tuples `(train_rows, test_rows)`, where
each component of the tuple is a rows selector that can be applied to a frame
with `n` rows. Some of these row selectors will be simple python ranges, others
will be single-column Frame objects.

The range `[0; n)` is split into `k` approximately equal parts (called "folds"),
and then each `i`-th split will use the `i`-th fold as a test part, and all the
remaining rows as the train part. Thus, `i`-th split is comprised of:

  - train: rows [0; i*n/k) + [(i+1)*n/k; n)
  - test:  rows [i*n/k; (i+1)*n/k)

where integer division is assumed.

Parameters
----------
k: int
    Number of folds, must be at least 2.

n: int
    The number of rows in the frame that you want to split. This parameter
    must be greater or equal to `k`.
)",

[](const py::PKArgs& args) -> py::oobj {
  if (!args[0]) throw TypeError() << "Required parameter `k` is missing";
  if (!args[1]) throw TypeError() << "Required parameter `n` is missing";
  size_t k = args[0].to_size_t();
  size_t n = args[1].to_size_t();
  if (k < 2) {
    throw ValueError() << "The number of splits `k` cannot be less than 2";
  }
  if (k > n) {
    throw ValueError() << "The number of splits `k` cannot exceed the number "
        "of rows in the frame";
  }

  int64_t ik = static_cast<int64_t>(k);
  int64_t in = static_cast<int64_t>(n);

  olist res(k);
  otuple split_first(2);
  split_first.set(0, orange(in/ik, in));
  split_first.set(1, orange(0, in/ik));
  otuple split_last(2);
  split_last.set(0, orange(0, (ik-1)*in/ik));
  split_last.set(1, orange((ik-1)*in/ik, in));
  res.set(0, split_first);
  res.set(k-1, split_last);

  std::vector<int32_t*> data;

  for (int64_t ii = 1; ii < ik-1; ++ii) {
    int64_t b1 = ii*in/ik;
    int64_t b2 = (ii+1)*in/ik;
    size_t colsize = static_cast<size_t>(b1 + in - b2);
    Column* col = Column::new_data_column(SType::INT32, colsize);
    DataTable* dt = new DataTable({col});
    data.push_back(static_cast<int32_t*>(col->data_w()));

    otuple split_i(2);
    split_i.set(0, oobj::from_new_reference(Frame::from_datatable(dt)));
    split_i.set(1, orange(b1, b2));
    res.set(static_cast<size_t>(ii), split_i);
  }

  for (int64_t t = 0; t < ik*(ik-2); ++t) {
    int64_t j = t / ik;     // column index
    int64_t i = t - j * ik; // block of rows
    ++j;
    if (i == j) continue;
    int64_t row0 = i * in / ik;
    int64_t row1 = (i+1) * in / ik;
    int64_t delta = i < j? 0 : (j+1)*in/ik - j*in/ik;
    int32_t* ptr = data[static_cast<size_t>(j) - 1] - delta;
    for (int64_t row = row0; row < row1; ++row) {
      ptr[row] = static_cast<int32_t>(row);
    }
  }

  return std::move(res);
});



}  // namespace py



void DatatableModule::init_methods_kfold() {
  ADDFN(py::fn_kfold_simple);
}
