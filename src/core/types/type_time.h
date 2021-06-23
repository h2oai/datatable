//------------------------------------------------------------------------------
// Copyright 2021 H2O.ai
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
#ifndef dt_TYPES_TYPE_TIME_h
#define dt_TYPES_TYPE_TIME_h
#include "types/typeimpl.h"
namespace dt {



/**
  * Time64 type represents a moment in time, which is stored as a
  * time offset from the epoch (1970-01-01T00:00:00Z), in nanoseconds.
  *
  * Additionally, the type carries time zone information as a meta
  * information [NYI]. In practice it means that all time moments
  * "belong" to the same time zone.
  *
  * In practice, this means the following:
  *   - if t is a time moment, then floor(t/(24*3600*1e9)) is the date
  *     offset of this time moment since the epoch;
  *   - time moments in different time zones can be compared directly
  *     without taking time zone information into account;
  *   - the time zone affects only conversion into local time/date,
  *     which in turn affects string representation.
  */
class Type_Time64 : public TypeImpl {
  // TODO: add timezone field

  public:
    Type_Time64();
    bool can_be_read_as_int64() const override;
    bool is_temporal() const override;
    std::string to_string() const override;
    py::oobj min() const override;
    py::oobj max() const override;
    const char* struct_format() const override;
    TypeImpl* common_type(TypeImpl* other) override;
    Column cast_column(Column&& col) const override;
};




}  // namespace dt
#endif
