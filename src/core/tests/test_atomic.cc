//------------------------------------------------------------------------------
// Copyright 2019-2022 H2O.ai
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
#ifdef DT_TEST
#include <atomic>       // std::atomic
#include <cmath>        // std::pow, std::abs
#include "parallel/api.h"
#include "parallel/atomic.h"
#include "utils/exceptions.h"
#include "utils/tests.h"
namespace dt {
namespace tests {


template <typename T>
static void test_atomic_impl() {
  // n must be signed, otherwise some expressions below overflow at n = 2
  int n = static_cast<int>(dt::num_threads_in_pool());
  std::atomic<int> barrier { n };

  dt::atomic<T> x { T(0.0) };
  dt::atomic<T> y { T(1.0) };
  dt::atomic<T> z { T(1.3e20) };
  std::atomic<int> q { 0 };
  std::atomic<int> r { 100 };

  dt::parallel_region(
    [&] {
      barrier--;
      while (barrier.load());
      int i = static_cast<int>(dt::this_thread_index());
      if (i >= n) {
        throw AssertionError() << "Invalid thread index: " << i;
      }

      x += static_cast<T>(i);
      y *= static_cast<T>(2.0);
      z /= static_cast<T>(1.5);
      x -= static_cast<T>(1.0);
      dt::atomic_fetch_max(&q, static_cast<int>(i));
      dt::atomic_fetch_min(&r, static_cast<int>(i));
    });

  T x_exp = static_cast<T>(n * (n - 3) / 2);
  T y_exp = static_cast<T>(std::pow(2.0, n));
  T z_exp = static_cast<T>(1.3e20);
  for (int i = 0; i < n; ++i) {
    z_exp /= static_cast<T>(1.5);
  }
  T x_act = x.load();
  T y_act = y.load();
  T z_act = z.load();
  int q_act = q.load();  // max
  int r_act = r.load();  // min
  int q_exp = n - 1;
  int r_exp = 0;

  ASSERT_FLOAT_EQ(x_act, x_exp);
  ASSERT_FLOAT_EQ(y_act, y_exp);
  ASSERT_FLOAT_EQ(z_act, z_exp);
  ASSERT_EQ(q_act, q_exp);
  ASSERT_EQ(r_act, r_exp);
}


TEST(parallel, atomic_float) {
  test_atomic_impl<float>();
}

TEST(parallel, atomic_double) {
  test_atomic_impl<double>();
}



}} // namespace dt::tests
#endif
