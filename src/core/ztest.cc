//------------------------------------------------------------------------------
// Copyright 2019 H2O.ai
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//------------------------------------------------------------------------------
#ifdef DTTEST
#include "datatablemodule.h"
#include "utils/exceptions.h"
#include "ztest.h"
namespace py {


static PKArgs arg_test_coverage(0, 0, 0, false, false, {}, "test_coverage");

static void test_coverage(const PKArgs&) {
  dttest::cover_init_FrameInitializationManager_em();
  dttest::cover_names_FrameNameProviders();
  dttest::cover_names_integrity_checks();
}


// Test progress reporting

static PKArgs arg_test_progress_static(2, 0, 0, false, false,
  {"n_iters", "n_threads"},
  "test_progress_static");

static void test_progress_static(const PKArgs& args) {
  args.check_posonly_args();
  size_t n_iters = args[0].to_size_t();
  size_t n_threads = args[1].to_size_t();
  dttest::test_progress_static(n_iters, n_threads);
}


static PKArgs arg_test_progress_nested(2, 0, 0, false, false,
  {"n_iters", "n_threads"},
  "test_progress_nested");

static void test_progress_nested(const PKArgs& args) {
  args.check_posonly_args();
  size_t n_iters = args[0].to_size_t();
  size_t n_threads = args[1].to_size_t();
  dttest::test_progress_nested(n_iters, n_threads);
}


static PKArgs arg_test_progress_dynamic(2, 0, 0, false, false,
  {"n_iters", "n_threads"},
  "test_progress_dynamic");

static void test_progress_dynamic(const PKArgs& args) {
  args.check_posonly_args();
  size_t n_iters = args[0].to_size_t();
  size_t n_threads = args[1].to_size_t();
  dttest::test_progress_dynamic(n_iters, n_threads);
}


static PKArgs arg_test_progress_ordered(2, 0, 0, false, false,
  {"n_iters", "n_threads"},
  "test_progress_ordered");

static void test_progress_ordered(const PKArgs& args) {
  args.check_posonly_args();
  size_t n_iters = args[0].to_size_t();
  size_t n_threads = args[1].to_size_t();
  dttest::test_progress_ordered(n_iters, n_threads);
}


void DatatableModule::init_tests() {
  ADD_FN(&test_coverage, arg_test_coverage);
  ADD_FN(&test_progress_static, arg_test_progress_static);
  ADD_FN(&test_progress_nested, arg_test_progress_nested);
  ADD_FN(&test_progress_dynamic, arg_test_progress_dynamic);
  ADD_FN(&test_progress_ordered, arg_test_progress_ordered);
}



} // namespace py




//------------------------------------------------------------------------------
// Helper functions
//------------------------------------------------------------------------------
namespace dttest {

/**
 * Check that calling `f()` causes an AssertionError with the specified
 * message string.
 */
void test_assert(const std::function<void(void)>& f,
                 const std::string& expected_error) {
  try {
    f();
  } catch (const std::exception& e) {
    exception_to_python(e);
    PyError pye;
    if (!pye.is_assertion_error()) throw pye;
    std::string error_message = pye.message();
    if (error_message.find(expected_error) == std::string::npos) {
      throw ValueError() << "Expected exception message `" << expected_error
          << "`, got `" << error_message << "`";
    }
    return;
  }
  throw ValueError() << "Assertion error `" << expected_error
      << "` was not raised";
}


}  // namespace dttest
#endif
