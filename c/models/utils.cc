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
#include <vector>
#include "models/utils.h"

/**
 *  For a given `n` calculate all the coprime numbers and return them
 *  as a `coprimes` vector.
 */
void calculate_coprimes(size_t n, intvec& coprimes) {
  coprimes.clear();
  if (n == 1) {
    coprimes.push_back(1);
    return;
  }

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
    if (mask[i - 1] == 0) {
      coprimes.push_back(i);
    }
  }
}
