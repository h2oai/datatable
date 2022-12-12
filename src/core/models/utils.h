//------------------------------------------------------------------------------
// Copyright 2018-2022 H2O.ai
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
#ifndef dt_MODELS_UTILS_h
#define dt_MODELS_UTILS_h
#include <algorithm>    // std::sort
#include <cmath>        // std::exp
#include <limits>
#include <memory>
#include <numeric>      // std::iota
#include <random>
#include "datatable.h"
#include "parallel/api.h"


template <typename T>
using tptr = typename std::unique_ptr<T[]>;
using uint64ptr = std::unique_ptr<uint64_t[]>;
using sizetptr = std::unique_ptr<size_t[]>;


/**
 *  Structure to store parameters of the modular quasi-random
 *  generator.
 */
struct ModularParams {
  size_t multiplier;
  size_t increment;
  ModularParams() : multiplier(1),
                    increment(0)
                    {}
};


ModularParams modular_random_gen(size_t, unsigned int);
sztvec calculate_coprimes(size_t);
size_t gcd(size_t, size_t);


/**
 *  Create list of sorting indexes.
 */
template <typename T>
sztvec sort_index(const std::vector<T> &v) {

  // Initialize original index locations
  sztvec index(v.size());
  std::iota(index.begin(), index.end(), 0);

  // Sort index based on comparing values in v
  std::sort(
    index.begin(),
    index.end(),
    [&v](size_t i1, size_t i2) {return v[i1] < v[i2];}
  );

  return index;
}


/**
 *  Sigmoid function.
 */
template<typename T>
inline T sigmoid(T x) {
  return T(1) / (T(1) + std::exp(-x));
}


/**
 *  Identity function.
 */
template<typename T>
inline T identity(T x) {
  return x;
}


/**
 *  Calculate logloss(p, y) = -(y * log(p) + (1 - y) * log(1 - p)),
 *  where p is a prediction and y is the actual target:
 *  - apply minmax rule, so that p falls into the [epsilon; 1 - epsilon] interval,
 *    to prevent logloss being undefined;
 *  - simplify the logloss formula to more compact branchless code.
 */
template<typename T>
inline T log_loss(T p, T y) {
  constexpr T epsilon = std::numeric_limits<T>::epsilon();
  p = std::max(std::min(p, 1 - epsilon), epsilon);
  return -std::log(p * (2*y - 1) + 1 - y);
}


/**
 *  Squared loss, T is either float or double,
 */
template<typename T>
inline T squared_loss(T p, T y) {
  return (p - y) * (p - y);
}


/**
 *  Normalize predictions, so that their values sum up to `1` row-wise.
 *  To prevent overflow when calculating the softmax function,
 *  we multiply its numerator and denominator by `std::exp(-max)`,
 *  where `max` is the maximum value of predictions for a given row.
 */
template <typename T>
void softmax(std::vector<T*>& p, const size_t nrows) {
  size_t ncols = p.size();

  dt::parallel_for_static(nrows, [&](size_t i){
    T sum = T(0);
    T max = p[0][i];
    for (size_t j = 1; j < ncols; ++j) {
      if (p[j][i] > max) max = p[j][i];
    }

    for (size_t j = 0; j < ncols; ++j) {
      p[j][i] = std::exp(p[j][i] - max);
      sum += p[j][i];
    }
    for (size_t j = 0; j < ncols; ++j) {
      p[j][i] /= sum;
    }
  });
}



/**
 *  Calculate integer powers via the exponentiation by squaring algorithm.
 *  Note, this implementation will compute zero to the power of zero as one.
 */
template <typename T>
T ipow(T base, size_t exp){
  T res = 1;
  while (exp) {
    if (exp % 2) {
      res *= base;
    }
    base *= base;
    exp /= 2;
  }
  return res;
}



/**
 *  Portable implementation of std::isinf.
 */
template <typename T> inline bool _isinf(T) { return false; }
template <> inline bool _isinf(float x) { return std::isinf(x); }
template <> inline bool _isinf(double x) { return std::isinf(x); }


/**
 *  Portable implementation of std::isfinite.
 */
template <typename T> inline bool _isfinite(T) { return true; }
template <> inline bool _isfinite(float x) { return std::isfinite(x); }
template <> inline bool _isfinite(double x) { return std::isfinite(x); }


/**
 * Portable implementation of !std::isnan.
 */
template <typename T> inline bool _notnan(T) { return true; }
template <> inline bool _notnan(float x) { return !std::isnan(x); }
template <> inline bool _notnan(double x) { return !std::isnan(x); }


/**
 * Convert `T` to `std::string`. In contrast to `std::to_string()`
 * this function won't produce any trailing zeros.
 */
template <typename T>
std::string tostr(const T& v) {
   std::ostringstream oss;
   oss << v;
   return oss.str();
}


// Helpers for parallelization
size_t get_work_amount(const size_t, const size_t);


#endif
