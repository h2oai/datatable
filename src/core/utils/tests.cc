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
#include <cstring>             // std::strlen, std::strncmp
#include <memory>              // std::unique_ptr
#include <string>              // std::string
#include <unordered_map>       // std::unordered_map
#include <utility>             // std::pair
#include <vector>              // std::vector
#include "datatablemodule.h"
#include "python/_all.h"
#include "python/args.h"
#include "utils/exceptions.h"
#include "utils/tests.h"
#ifdef DTTEST
namespace dt {
namespace tests {

using testfn = void(*)(); //std::function<void()>;
using strvec = std::vector<std::string>;
using TestSuite = std::vector<TestCase*>;
using TestRegistry = std::unordered_map<std::string, TestSuite>;


//------------------------------------------------------------------------------
// Tests registry
//------------------------------------------------------------------------------

// This cannot be a simple global variable, because the order of initialization
// of static variables is not well-defined, and it is possible that some of the
// `Register` objects would be created before the TestsRegistry is initialized.
static TestRegistry& get_tests_registry() {
  static TestRegistry all_tests;
  return all_tests;
}


static strvec get_suites_list() {
  strvec result;
  for (const auto& entry : get_tests_registry()) {
    result.push_back(entry.first);
  }
  return result;
}


static TestSuite& get_suite(const std::string& suite) {
  if (!get_tests_registry().count(suite)) {
    throw KeyError() << "Test suite `" << suite << "` does not exist";
  }
  return get_tests_registry()[suite];
}


static strvec get_tests_list(const std::string& suite) {
  strvec result;
  for (const auto& entry : get_suite(suite)) {
    result.push_back(entry->name());
  }
  return result;
}


static void run_test(const std::string& suite, const std::string& name) {
  for (const auto& entry : get_suite(suite)) {
    if (entry->name() == name) {
      entry->xrun();
      return;
    }
  }
  throw KeyError()
      << "Test `" << name << "` does not exist in test suite `" << suite << "`";
}


TestCase::TestCase(const char* suite, const char* test, const char* file)
  : suite_name_(suite),
    test_name_(test),
    file_name_(file)
{
  get_tests_registry()[suite_name_].emplace_back(this);
}



void assert_throws(std::function<void()> expr,
                   const char* message, const char* filename, int lineno)
{
  assert_throws(expr, nullptr, message, filename, lineno);
}

void assert_throws(std::function<void()> expr, Error(*exception_class)(),
                   const char* filename, int lineno)
{
  assert_throws(expr, exception_class, nullptr, filename, lineno);
}

void assert_throws(std::function<void()> expr, Error(*exception_class)(),
                   const char* message, const char* filename, int lineno)
{
  try {
    expr();

  } catch (const Error& e) {
    std::string emsg = e.to_string();
    if (exception_class && !e.matches_exception_class(exception_class)) {
      throw AssertionError()
          << "Wrong exception class thrown in " << filename << ':' << lineno
          << ": " << emsg;
    }
    if (message && std::strncmp(emsg.data(), message, std::strlen(message))) {
      throw AssertionError()
          << "Wrong exception message in " << filename << ':' << lineno
          << "\n  Actual:   " << emsg
          << "\n  Expected: " << message;
    }
    return;
  }
  throw AssertionError()
      << "Exception was not thrown in " << filename << ':' << lineno;
}




}}  // namespace dt::test
//------------------------------------------------------------------------------
// Python API
//------------------------------------------------------------------------------
namespace py {


static const char* doc_get_test_suites =
R"(get_test_suites()
--

Returns the list of all test suites, i.e. the list of "categories"
of tests.
)";

static PKArgs arg_get_test_suites(
    0, 0, 0, false, false, {}, "get_test_suites", doc_get_test_suites);

static oobj get_test_suites(const PKArgs&) {
  auto suites = dt::tests::get_suites_list();
  olist result(suites.size());
  for (size_t i = 0; i < suites.size(); ++i) {
    result.set(i, ostring(suites[i]));
  }
  return std::move(result);
}


static const char* doc_get_tests_in_suite =
R"(get_tests_in_suite(suite)
--

Returns the list of tests within a particular suite. An error will
be raised if the requested test suite does not exist.
)";

static PKArgs arg_get_tests_in_suite(
    1, 0, 0, false, false, {"suite"}, "get_tests_in_suite",
    doc_get_tests_in_suite);

static oobj get_tests_in_suite(const PKArgs& args) {
  std::string suite = args[0].to_string();
  auto tests = dt::tests::get_tests_list(suite);
  olist result(tests.size());
  for (size_t i = 0; i < tests.size(); ++i) {
    result.set(i, ostring(tests[i]));
  }
  return std::move(result);
}


static const char* doc_run_test =
R"(run_test(suite, test)
--

Run a test within the test suite. The test does not return anything,
but an error will be thrown if something goes wrong.
)";

static PKArgs arg_run_test(
    2, 0, 0, false, false, {"suite", "test"}, "run_test", doc_run_test);

static void run_test(const PKArgs& args) {
  std::string suite = args[0].to_string();
  std::string test  = args[1].to_string();
  dt::tests::run_test(suite, test);
}


void DatatableModule::init_tests() {
  ADD_FN(&get_test_suites, arg_get_test_suites);
  ADD_FN(&get_tests_in_suite, arg_get_tests_in_suite);
  ADD_FN(&run_test, arg_run_test);
}



}  // namespace py
#endif
