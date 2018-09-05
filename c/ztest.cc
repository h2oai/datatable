//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#ifdef DTTEST
#include "ztest.h"
#include "utils/exceptions.h"
namespace dttest {


void run_tests() {
  cover_init_FrameInitializationManager_em();
  cover_names_FrameNameProviders();
  cover_names_integrity_checks();
}


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
      throw ValueError() << "Expected exception message <<" << expected_error
          << ">>, got <<" << error_message << ">>";
    }
    return;
  }
  throw ValueError() << "Assertion error <<" << expected_error
      << ">> was not raised";
}


}  // namespace dttest
#endif
