//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#ifdef DTTEST
#include "datatablemodule.h"
#include "python/args.h"
#include "utils/exceptions.h"
#include "ztest.h"
namespace py {


static PKArgs arg_test_coverage(0, 0, 0, false, false, {}, "test_coverage");

static void test_coverage(const PKArgs&) {
  dttest::cover_init_FrameInitializationManager_em();
  dttest::cover_names_FrameNameProviders();
  dttest::cover_names_integrity_checks();
}

static PKArgs arg_test_shmutex(3, 0, 0, false, false,
  {"n_iters", "n_threads", "impl"},
  "test_shmutex");

static void test_shmutex(const PKArgs& args) {
  size_t n_iters = args[0].to_size_t();
  size_t n_threads = args[1].to_size_t();
  int impl = args[2].to_int32_strict();
  dttest::test_shmutex(n_iters, n_threads, impl);
}



void DatatableModule::init_tests() {
  ADD_FN(&test_coverage, arg_test_coverage);
  ADD_FN(&test_shmutex, arg_test_shmutex);
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
