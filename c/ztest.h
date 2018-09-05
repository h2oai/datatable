//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#ifndef dt_ZTEST_h
#define dt_ZTEST_h
#ifdef DTTEST
#include <functional>
#include <string>

namespace dttest {


void run_tests();
void test_assert(const std::function<void(void)>&, const std::string&);

void cover_init_FrameInitializationManager_em();
void cover_names_FrameNameProviders();
void cover_names_integrity_checks();


}  // namespace dttest

#endif
#endif
