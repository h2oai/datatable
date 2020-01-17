//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#ifndef dt_READ_FIELD64_h
#define dt_READ_FIELD64_h
#include <cstdint>  // int32_t, INT32_MIN, ...

namespace dt {
namespace read {


/**
 * "Relative string": a string defined as an offset+length relative to some
 * anchor point (which has to be provided separately). This is the internal
 * data structure for reading strings from a file.
 */
struct RelStr {
  uint32_t offset;
  int32_t length;

  bool isna() { return length == INT32_MIN; }
  void setna() { length = INT32_MIN; }
};



union field64 {
  int8_t   int8;
  int32_t  int32;
  int64_t  int64;
  uint8_t  uint8;
  uint32_t uint32;
  uint64_t uint64;
  float    float32;
  double   float64;
  RelStr   str32;
};


}  // namespace read
}  // namespace dt

#endif
