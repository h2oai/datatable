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


// Don't know why, but without this `make coverage` fails.
void foo();

}} // namespace dt::read::
#endif
