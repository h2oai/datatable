//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
//
// This file should only be included from `datatablemodule.cc`
//
#ifdef DTTEST
#include "frame/py_frame.h"

namespace dttest {

void run_tests();


void run_tests() {
  py::cover_py_FrameInitializationManager_em();
  py::cover_py_FrameNameProviders();
}


}  // namespace dttest

#endif
