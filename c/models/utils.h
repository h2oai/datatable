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
#ifndef dt_MODELS_UTILS_h
#define dt_MODELS_UTILS_h
#include "py_datatable.h"

template <typename T>
using tptr = typename std::unique_ptr<T[]>;
using uint64ptr = std::unique_ptr<uint64_t[]>;

/*
*  Helper template structures to convert C++ float/double types to
*  datatable STypes::FLOAT32/STypes::FLOAT64. respectively.
*/
template<typename T> struct stype {
  static void get_stype() {
    throw TypeError() << "Only float and double types are supported";
  }
};


template<> struct stype<float> {
  static SType get_stype() {
    return SType::FLOAT32;
  }
};


template<> struct stype<double> {
  static SType get_stype() {
    return SType::FLOAT64;
  }
};

void calculate_coprimes(size_t, std::vector<size_t>&);

/*
*  Sigmoid function.
*/
template<typename T>
inline T sigmoid(T x) {
  T one = static_cast<T>(1.0);
  return one / (one + std::exp(-x));
}

/*
*  Identity function.
*/
template<typename T>
inline T identity(T x) {
  return x;
}

/*
* Calculate logloss(p, y) = -(y * log(p) + (1 - y) * log(1 - p)),
* where p is a prediction and y is the actual target:
* - apply minmax rule, so that p falls into the [epsilon; 1 - epsilon] interval,
*   to prevent logloss being undefined;
* - simplify the logloss formula to more compact branchless code.
*/
template<typename T>
inline T log_loss(T p, bool y) {
  T epsilon = std::numeric_limits<T>::epsilon();
  p = std::max(std::min(p, 1 - epsilon), epsilon);
  return -std::log(p * (2*y - 1) + 1 - y);
}


/*
* Squared loss.
*/
template<typename T1, typename T2>
inline T1 squared_loss(T1 p, T2 y) {
  return (p - y) * (p - y);
}

/*
* Implementation of `make_unique` template function.
* Note: this implementation does not disable this overload for array types,
* see https://en.cppreference.com/w/cpp/memory/unique_ptr/make_unique
*/
template<typename T, typename... Args>
std::unique_ptr<T> make_unique(Args&&... args) {
  return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

/*
* Progress reporting function and parameters.
*/
#define PBSTR "||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||"
#define PBWIDTH 60
void print_progress(float, int);

#endif
