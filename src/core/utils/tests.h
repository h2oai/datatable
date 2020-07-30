//------------------------------------------------------------------------------
// Copyright 2020 H2O.ai
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
#ifndef dt_UTILS_TESTS_h
#define dt_UTILS_TESTS_h
#include <functional>      // std::function
namespace dt {
namespace tests {
#ifdef DTTEST

class Register {
  public:
    Register(std::function<void()> fn, const char* suite, const char* name);
};


#define TEST(suite, name)  \
  static void test_##suite##_##name(); \
  static dt::tests::Register \
            reg_##suite##_##name(test_##suite##_##name, #suite, #name); \
  void test_##suite##_##name()




#else  // ifdef DTTEST

#define TEST(suite, name)  \
  static_assert(0, "TEST() macro should not be used when DTTEST not defined");

#endif
}}  // namespace dt::tests
#endif
