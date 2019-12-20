//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#ifndef dt_READ_CONSTANTS_h
#define dt_READ_CONSTANTS_h
#include <cstdint>
namespace dt {
namespace read {

extern const uint8_t hexdigits[256];
extern const uint8_t allowedseps[128];
extern const long double pow10lookup[701];


// FIXME: this is a temporary fix, so that `make coverage` do not fail.
// The problem is that if there are no function definitions in the `.cc` file,
// then `ci/llvm-gcov.sh` is invoked from the wrong directory.
// This is because we are using `lcov` that in its turn invokes `geninfo`,
// and when `geninfo` is trying to detect the base directory in the
// `find_base_from_graph()` routine — it fails.
// The reason seems to be that if a particular `.cc` file contains
// no function definitions, it is excluded from the file graph
// where this routine does the search.
void foo();

}} // namespace dt::read::
#endif
