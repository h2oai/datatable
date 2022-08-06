//------------------------------------------------------------------------------
// Copyright 2020-2021 H2O.ai
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
//
// Simplified test framework. Use as follows:
//
// In a C++ file:
//
//      #include "utils/tests.h"
//
//      TEST(suite_name, test_name) {
//        ... test body ...
//        ... throw an exception here if something goes wrong ...
//      }
//
// Neither `suite_name` nor `test_name` should be put in quotes. In
// fact, they should both be valid identifiers.
// The `suite_name` is any name that organizes the test into a
// collection of similar tests. Thus, all tests form a single-level
// hierarchy with several suites and multiple tests within each
// suite. This is quite similar to how pytest organizes tests into
// files, with multiple tests per file.
//
// After a test is declared in a C++ source file, it becomes
// automatically available within python via API
//
//   core.get_test_suites()                # -> List[str]
//   core.get_tests_in_suite(suite: str)   # -> List[str]
//   core.run_test(suite: str, test: str)  # -> None
//
// It is thus easy to setup pytest framework to automatically find
// and execute all these tests.
//
//------------------------------------------------------------------------------
//
// Several assert macros are also available to simplify writing of tests:
//
//   ASSERT_EQ(x, y)        # assert x == y
//   ASSERT_NE(x, y)        # assert x != y
//   ASSERT_LT(x, y)        # assert x < y
//   ASSERT_GT(x, y)        # assert x > y
//   ASSERT_LE(x, y)        # assert x <= y
//   ASSERT_GE(x, y)        # assert x >= y
//   ASSERT_FLOAT_EQ(x, y)  # assert isclose(x, y)  # up to 4 ulps
//   ASSERT_TRUE(s)         # assert s
//   ASSERT_FALSE(s)        # assert not s
//
// In addition, these asserts allow working with code that throws exceptions.
// Here function `fn()` is executed, which must throw an exception
//   1) of class `cls`
//   2) starting with message `msg`
//   3) having class `cls` and starting with message `msg`
//
//   ASSERT_THROWS(fn, cls)
//   ASSERT_THROWS(fn, msg)
//   ASSERT_THROWS(fn, cls, msg)
//
//------------------------------------------------------------------------------
#ifndef dt_UTILS_TESTS_h
#define dt_UTILS_TESTS_h
#include <cmath>                // std::isnan
#include <functional>           // std::function
#include <type_traits>          // std::is_floating_point
#include "utils/exceptions.h"
#include "utils/macros.h"
namespace dt {
namespace tests {

#ifndef DT_TEST
#define TEST(suite, name)  \
  static_assert(0, "TEST() macro should not be used when DT_TEST is not defined");
#else

// C++ tests always need access to private fields and methods, which
// is why we turn all access public when compiling with tests. This
// has an obvious drawback though: in debug mode the access control
// is no longer verified by the compiler, which means it's easier to
// inadvertently introduce a compiler error. Such errors would be
// revealed by building in the production mode ("make build").
DISABLE_CLANG_WARNING("-Wkeyword-macro")
#define private   public
#define protected public
RESTORE_CLANG_WARNING("-Wkeyword-macro")


//------------------------------------------------------------------------------
// Internal
//------------------------------------------------------------------------------

class TestCase {
  protected:
    const char* suite_name_;
    const char* test_name_;
    const char* file_name_;

  public:
    TestCase(const char* suite, const char* test, const char* file);
    virtual ~TestCase() = default;

    const char* suite() const { return suite_name_; }
    const char* name() const { return test_name_; }
    virtual void xrun() = 0;
};

#define TESTNAME(suite, name) test_##suite##_##name

#ifndef ___STRINGIFY
  #define ___STRINGIFY(TEXT) #TEXT
#endif

template <typename T>
AutoThrowingError assert_cmp(bool ok, T x, T y, const char* xstr, const char* ystr,
                const char* opstr, const char* filename, int lineno)
{
  if (ok) return AutoThrowingError();
  Error err = AssertionError();
  err << xstr << opstr << ystr << " failed in " << filename << ":" << lineno
      << ", where lhs = " << x << " and rhs = " << y;
  return AutoThrowingError(std::move(err));
}

template <typename T>
void assert_float_eq(T x, T y, const char* xstr, const char* ystr,
                     const char* filename, int lineno)
{
  static_assert(std::is_floating_point<T>::value,
                "Invalid argument in assert_float_eq()");
  if (x == y) return;
  if (std::isnan(x) && std::isnan(y)) return;
  T z = std::nextafter(x, y);  // 1 ulp
  if (z == y) return;
  z = std::nextafter(z, y);    // 2 ulps
  if (z == y) return;
  z = std::nextafter(z, y);    // 3 ulps
  if (z == y) return;
  z = std::nextafter(z, y);    // 4 ulps
  if (z == y) return;
  throw AssertionError() << '(' << xstr << ")==(" << ystr << ')'
      << " failed in " << filename << ":" << lineno << ", where "
      << "lhs = " << x << " and rhs = " << y;
}

template <bool EXP>
void assert_bool(bool arg, const char* argstr, const char* filename, int lineno)
{
  if (arg == EXP) return;
  throw AssertionError() << (EXP? "" : "!") << '(' << argstr << ") failed in "
            << filename << ':' << lineno;
}

void assert_throws(std::function<void()> expr, Error(*exception_class)(),
                   const char* filename, int lineno);

void assert_throws(std::function<void()> expr,
                   const char* message, const char* filename, int lineno);

void assert_throws(std::function<void()> expr, Error(*exception_class)(),
                   const char* message, const char* filename, int lineno);



//------------------------------------------------------------------------------
// Public API
//------------------------------------------------------------------------------

#define TEST(suite, name)                                                      \
    class TESTNAME(suite, name) : public dt::tests::TestCase {                 \
      public:                                                                  \
        using dt::tests::TestCase::TestCase;                                   \
        void xrun() override;                                                  \
    };                                                                         \
    static TESTNAME(suite, name) testcase_##suite##_##name                     \
                                        (#suite, #name, __FILE__);             \
    void TESTNAME(suite, name)::xrun()


#define ASSERT_EQ(x, y)                                                        \
    dt::tests::assert_cmp(((x)==(y)), x, y, ___STRINGIFY(x), ___STRINGIFY(y),  \
                          " == ", __FILE__, __LINE__)

#define ASSERT_NE(x, y)                                                        \
    dt::tests::assert_cmp(((x)!=(y)), x, y, ___STRINGIFY(x), ___STRINGIFY(y),  \
                          " != ", __FILE__, __LINE__)

#define ASSERT_LT(x, y)                                                        \
    dt::tests::assert_cmp(((x)<(y)), x, y, ___STRINGIFY(x), ___STRINGIFY(y),   \
                          " < ", __FILE__, __LINE__)

#define ASSERT_GT(x, y)                                                        \
    dt::tests::assert_cmp(((x)>(y)), x, y, ___STRINGIFY(x), ___STRINGIFY(y),   \
                          " > ", __FILE__, __LINE__)

#define ASSERT_LE(x, y)                                                        \
    dt::tests::assert_cmp(((x)<=(y)), x, y, ___STRINGIFY(x), ___STRINGIFY(y),  \
                          " <= ", __FILE__, __LINE__)

#define ASSERT_GE(x, y)                                                        \
    dt::tests::assert_cmp(((x)>=(y)), x, y, ___STRINGIFY(x), ___STRINGIFY(y),  \
                          " >= ", __FILE__, __LINE__)

// Similar to ASSERT_EQ(x, y), but for floating-point arguments:
//   - NaNs compare equal to each other;
//   - the values x and y are considered equal if they are within 4 ulps
//     from each other.
#define ASSERT_FLOAT_EQ(x, y)                                                  \
    dt::tests::assert_float_eq(x, y, ___STRINGIFY(x), ___STRINGIFY(y),         \
                               __FILE__, __LINE__)


#define ASSERT_TRUE(s)                                                         \
    dt::tests::assert_bool<true>(s, ___STRINGIFY(s), __FILE__, __LINE__)


#define ASSERT_FALSE(s)                                                        \
    dt::tests::assert_bool<false>(s, ___STRINGIFY(s), __FILE__, __LINE__)


#define ASSERT_THROWS(...)                                                     \
    dt::tests::assert_throws(__VA_ARGS__, __FILE__, __LINE__)



#endif  // ifdef DT_TEST
}}      // namespace dt::tests
#endif
