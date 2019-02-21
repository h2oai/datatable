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

void calculate_coprimes(size_t, std::vector<size_t>&);

/*
*  Sigmoid function.
*/
inline double sigmoid(double x) {
  return 1.0 / (1.0 + std::exp(-x));
}

/*
*  Identity function.
*/
inline double identity(double x) {
  return x;
}

/*
* Calculate logloss(p, y) = -(y * log(p) + (1 - y) * log(1 - p)),
* where p is a prediction and y is the actual target:
* - apply minmax rule, so that p falls into the [epsilon; 1 - epsilon] interval,
*   to prevent logloss being undefined;
* - simplify the logloss formula to more compact branchless code.
*/
inline double logloss(double p, bool y) {
  double epsilon = std::numeric_limits<double>::epsilon();
  p = std::max(std::min(p, 1 - epsilon), epsilon);
  return -std::log(p * (2*y - 1) + 1 - y);
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
