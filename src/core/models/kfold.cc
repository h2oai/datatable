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
#include <algorithm> // std::min
#include <cmath>     // std::sqrt
#include <numeric>   // std::accumulate
#include <random>    // std::random_device, std::normal_distribution
#include <vector>    // std::vector
#include "frame/py_frame.h"
#include "parallel/api.h"
#include "python/_all.h"
#include "python/arg.h"
#include "utils/exceptions.h"
#include "utils/function.h"
#include "column.h"
#include "datatable.h"
#include "datatablemodule.h"

namespace py {

//------------------------------------------------------------------------------
// Misc. helper functions
//------------------------------------------------------------------------------

static void extract_args(const PKArgs& args,
                         size_t* out_nrows, size_t* out_nsplits)
{
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
  *out_nrows = nrows;
  *out_nsplits = nsplits;
}


// Draws random samples from a HyperGeometric[N, K, n] distribution
// with parameters
//     N = population_size
//     K = positive_size
//     n = num_draws
// (see https://en.wikipedia.org/wiki/Hypergeometric_distribution)
//
// We approximate the exact HyperGeometric distribution with normal,
// since it's much easier to compute, and we do not care much about
// drawing from the exact HyperGeometric distribution, in the sense
// that if our approximation spreads the observation slightly more
// evenly across chunks than afforded by pure chance, then it will
// not deteriorate the resulting random folds in any way.
//
static size_t hypergeom(dt::function<double(void)> rnd,
                        size_t population_size,
                        size_t positive_size,
                        size_t num_draws)
{
  xassert(population_size >= positive_size);
  xassert(population_size >= num_draws);
  if (population_size == positive_size) return num_draws;
  if (population_size == num_draws) return positive_size;
  if (num_draws == 0 || positive_size == 0) return 0;
  double n = static_cast<double>(population_size);
  double k = static_cast<double>(positive_size);
  double m = static_cast<double>(num_draws);
  double mean = m*k / n;
  double var = mean * (n - k)*(n - m)/(n*(n-1));
  double z = rnd();
  double x = mean + std::sqrt(var) * z;
  if (x < 0) x = 0;
  size_t ret = static_cast<size_t>(x + 0.5);
  if (ret > positive_size) ret = positive_size;
  if (ret > num_draws) ret = num_draws;
  if (positive_size + num_draws > population_size) {
    size_t delta = positive_size + num_draws - population_size;
    if (ret < delta) ret = delta;
  }
  return ret;
}



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
  size_t nrows, nsplits;
  extract_args(args, &nrows, &nsplits);

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
    Column col = Column::new_data_column(colsize, SType::INT32);
    data.push_back(static_cast<int32_t*>(col.get_data_editable()));
    DataTable* dt = new DataTable({std::move(col)}, DataTable::default_names);

    res.set(static_cast<size_t>(ii),
            otuple(Frame::oframe(dt), orange(b1, b2)));
  }

  size_t kk = nsplits;
  size_t nn = nrows;
  dt::parallel_for_dynamic(kk*(kk-2),
    [&](size_t t) {
      size_t j = t / kk;     // column index
      size_t i = t - j * kk; // block of rows
      ++j;
      if (i == j) return;
      size_t row0 = i * nn / kk;
      size_t row1 = (i+1) * nn / kk;
      size_t delta = i < j? 0 : (j+1)*nn/kk - j*nn/kk;
      int32_t* ptr = data[static_cast<size_t>(j) - 1] - delta;
      for (size_t row = row0; row < row1; ++row) {
        ptr[row] = static_cast<int32_t>(row);
      }
    });

  return std::move(res);
}




//------------------------------------------------------------------------------
// kfold_random()
//------------------------------------------------------------------------------

static PKArgs args_kfold_random(
  0, 0, 3, false, false,
  {"nrows", "nsplits", "seed"}, "kfold_random",

R"(kfold_random(nrows, nsplits, seed=None)
--

Computes randomized k-fold split of data with `nrows` rows into
`nsplits` train/test subsets.

The dataset itself is not passed to this function: it is sufficient
to know only the number of rows in order to decide how the data should
be divided. Instead, this function returns a list of `nsplits` tuples,
each tuple containing `(train_rows, test_rows)`. Here `train_rows` and
`test_rows` are "row selectors": they can be applied to any frame with
`nrows` rows to select the desired folds.

The train/test subsets produced by this function will have these
properties:
  - All test folds will be of approximately same size nrows/nsplits;
  - All observations have equal ex-ante chance of getting assigned
    into each fold;
  - The row indices in all train and test folds will be sorted.

The function uses single-pass parallelized algorithm to construct the
folds.

Parameters
----------
nrows: int
    The number of rows in the frame that you want to split.

nsplits: int
    Number of folds, must be at least 2, but not larger than `nrows`.

seed: int (optional)
    Seed value for the random number generator used by this function.
    Calling ``kfold_random()`` several times with the same seed values
    will produce same results each time.
)");


static constexpr size_t CHUNK_SIZE = 4096;

inline static size_t chunk_start(size_t i, size_t nchunks, size_t nrows) {
  return (i * nrows / nchunks);
}


static oobj kfold_random(const PKArgs& args) {
  size_t nrows, nsplits;
  extract_args(args, &nrows, &nsplits);

  size_t seed = 0;
  if (args[2].is_none_or_undefined()) {
    std::random_device rd;
    seed = rd();
  } else {
    seed = args[2].to_size_t();
  }

  // The data will be processed in parallel, split by rows into
  // `nchunks` chunks. Each chunk has size at least 1, and is comprised
  // of rows `[i * nrows / nchunks .. (i + 1) * nrows / nchunks)`.
  // The number or chunks may not be a function of the number of threads,
  // because that would cause the random assignment to be non-reproducible.
  size_t nchunks = nrows / CHUNK_SIZE;
  if (nchunks == 0) nchunks = 1;

  // Now, we want each fold to have predetermined size of approximately
  // `nrows/nsplits` (up to round-off error, more precisely each fold's
  // size is given by `fold_size` computed below).
  // Within each chunk `i`, there will be a random number `s[x,i]`
  // of rows assigned to fold `x`. These random numbers satisfy the
  // conditions
  //
  //     sum(s[x,i] for i in range(nchunks)) == fold_size(x)
  //     sum(s[x,i] for x in range(nsplits)) == chunk_size(i)
  //
  std::vector<std::vector<size_t>> s(nsplits);
  {
    std::normal_distribution<> dis;
    std::mt19937 gen;
    gen.seed(static_cast<uint32_t>(seed));
    auto rnd = [&]{ return dis(gen); };

    std::vector<size_t> rows_in_chunk(nchunks);
    for (size_t i = 0; i < nchunks; ++i) {
      rows_in_chunk[i] = chunk_start(i + 1, nchunks, nrows) -
                         chunk_start(i, nchunks, nrows);
    }
    for (size_t x = 0; x < nsplits; ++x) {
      size_t fold_size = (x + 1) * nrows / nsplits - x * nrows / nsplits;
      size_t total = std::accumulate(rows_in_chunk.begin(),
                                     rows_in_chunk.end(), size_t(0));
      s[x].resize(nchunks);
      for (size_t i = 0; i < nchunks; ++i) {
        size_t v = hypergeom(rnd, total, rows_in_chunk[i], fold_size);
        s[x][i] = v;
        total -= rows_in_chunk[i];
        rows_in_chunk[i] -= v;
        fold_size -= v;
      }
    }
    #ifndef NDEBUG
      for (size_t i = 0; i < nchunks; ++i) {
        size_t chunk_size = chunk_start(i + 1, nchunks, nrows) -
                            chunk_start(i, nchunks, nrows);
        size_t actual = 0;
        for (size_t x = 0; x < nsplits; ++x) actual += s[x][i];
        xassert(actual == chunk_size);
      }
    #endif
  }

  // Cumulative sums of `s[x,i]` across each fold.
  std::vector<std::vector<size_t>> cums(nsplits);
  for (size_t x = 0; x < nsplits; ++x) {
    cums[x].resize(nchunks);
    cums[x][0] = s[x][0];
    for (size_t i = 1; i < nchunks; ++i) {
      cums[x][i] = cums[x][i - 1] + s[x][i];
    }
    xassert(cums[x][nchunks - 1] ==
            (x + 1) * nrows / nsplits - x * nrows / nsplits);
  }

  // Create data arrays for each fold
  using T = int32_t;
  SType S = SType::INT32;
  std::vector<T*> test_folds(nsplits);
  std::vector<T*> train_folds(nsplits);

  olist res(nsplits);
  for (size_t x = 0; x < nsplits; ++x) {
    size_t fold_size = (x + 1) * nrows / nsplits - x * nrows / nsplits;
    Column col1 = Column::new_data_column(nrows - fold_size, S);
    Column col2 = Column::new_data_column(fold_size, S);
    train_folds[x] = static_cast<T*>(col1.get_data_editable());
    test_folds[x] = static_cast<T*>(col2.get_data_editable());
    DataTable* dt1 = new DataTable({std::move(col1)}, DataTable::default_names);
    DataTable* dt2 = new DataTable({std::move(col2)}, DataTable::default_names);
    oobj train = Frame::oframe(dt1);
    oobj test  = Frame::oframe(dt2);
    res.set(x, otuple{ train, test });
  }

  // Fill-in the folds arrays
  dt::parallel_for_dynamic(
    /* niterations = */ nchunks,
    [&](size_t i) {
      size_t row0 = chunk_start(i, nchunks, nrows);
      size_t row1 = chunk_start(i + 1, nchunks, nrows);

      // Copy into a thread-private array so that arrays don't trample
      // each other's cachelines.
      std::vector<size_t> xcounts(nsplits);
      for (size_t x = 0; x < nsplits; ++x) xcounts[x] = s[x][i];
      // Each chunk starts from a predetermined seed value
      std::mt19937 gen;
      gen.seed(static_cast<uint32_t>(seed + i * 134368501));
      std::uniform_int_distribution<size_t> dis(0, nsplits - 1);

      for (size_t j = row0; j < row1; ++j) {
        size_t x = dis(gen);
        while (xcounts[x] == 0) {
          x++;
          if (x == nsplits) x = 0;
        }

        // row `j` is assigned to (test) fold `x`
        // `cums[x][i]` is how many rows will go into fold `x` for all chunks
        // up to and including `i`-th.
        // `xcounts[x]` is how many rows this chunk still has to assign to
        // fold `x`.
        // `cums[x][i] - xcounts[x]` is the position where this row `j` should
        // be recorded within the fold `x`.
        //
        test_folds[x][cums[x][i] - xcounts[x]] = static_cast<T>(j);
        xcounts[x]--;

        // row `j` must also appear in all train folds != x
        // This chunk will assign `chunk_size(i) - s[y,i]` rows into train
        // fold `y` (where `chunk_size(i) = row1 - row0`).
        // All previous chunks together with this one will assign
        //     sum(chunk_size(ii) - s[y,ii] for ii in range(i))
        //     = row1 - cums[y,i]
        // rows into the train fold `y`.
        // `row1 - j - xcounts[y]` is how many rows this chunk still has to
        // assign to train fold `y`.
        // Thus, row `j` should be written at index
        //     j + xcounts[y] - cums[y,i]
        //
        for (size_t y = 0; y < nsplits; ++y) {
          if (y == x) continue;
          train_folds[y][j + xcounts[y] - cums[y][i]] = static_cast<T>(j);
        }
      }
    }
  );

  return std::move(res);
}



void DatatableModule::init_methods_kfold() {
  ADD_FN(&kfold, args_kfold_simple);
  ADD_FN(&kfold_random, args_kfold_random);
}


} // namespace py
