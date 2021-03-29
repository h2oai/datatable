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
#include <vector>
#include "models/utils.h"
#include "parallel/api.h"


/**
 *  Generate a random multiplier and increment for quasi-random generator
 *  based on the modular arithmentics.
 */
ModularParams modular_random_gen(size_t n, unsigned int seed) {
  ModularParams mp;
  auto multipliers = calculate_coprimes(n);
  std::default_random_engine gen(seed);
  std::uniform_int_distribution<size_t> multiplier_dist(0, multipliers.size() - 1);
  std::uniform_int_distribution<size_t> increment_dist(0, n - 1);
  mp.multiplier = multipliers[multiplier_dist(gen)];
  mp.increment = increment_dist(gen);
  return mp;
}


/**
 *  For a given `n` calculate all the coprime numbers and return them
 *  as a `coprimes` vector.
 */
sztvec calculate_coprimes(size_t n) {
  sztvec coprimes;
  if (n == 1) {
    coprimes.push_back(1);
  } else {
    std::vector<bool> mask(n - 1, false);
    for (size_t i = 2; i <= n / 2; ++i) {
      if (mask[i - 1]) continue;
      if (n % i == 0) {
        size_t j = 1;
        while (j * i < n) {
          mask[j * i - 1] = true;
          j++;
        }
      }
    }

    for (size_t i = 1; i < n; ++i) {
      if (mask[i - 1] == false) {
        coprimes.push_back(i);
      }
    }
  }
  return coprimes;
}



/**
 *  Calculate work amount, i.e. the number of rows, to be processed
 *  by the zeroth thread for a `min_rows_per_thread` chunk size.
 */
size_t get_work_amount(const size_t nrows, const size_t min_rows_per_thread) {
  dt::NThreads nthreads(dt::nthreads_from_niters(nrows, min_rows_per_thread));
  size_t nth = nthreads.get();

  size_t chunk_rows = min_rows_per_thread * (nrows / (nth * min_rows_per_thread));
  size_t residual_rows = std::min(nrows - chunk_rows * nth, min_rows_per_thread);
  return chunk_rows + residual_rows;
}
