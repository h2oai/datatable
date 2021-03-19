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
//
// The algorithms listed here are borrowed from Howard Hinnant's
// http://howardhinnant.github.io/date_algorithms.html
//
//------------------------------------------------------------------------------
#include "lib/hh/date.h"
namespace hh {


// Returns number of days since epoch 1970-01-01. Negative values indicate
// days prior to 1970-01-01.
//
// Preconditions:
//   y-m-d represents a date in the proleptic Gregorian calendar
//   m is in [1, 12]
//   d is in [1, last_day_of_month(y, m)]
//   y is "approximately" in
//     [numeric_limits<int>::min()/366, numeric_limits<int>::max()/366]
//   Exact range of validity is:
//     [civil_from_days(numeric_limits<int>::min()),
//      civil_from_days(numeric_limits<int>::max()-719468)]
//
int days_from_civil(int y, int m, int d) noexcept {
  y -= (m <= 2);
  const int era = (y >= 0 ? y : y-399) / 400;
  const int yoe = y - era * 400;                             // [0, 399]
  const int doy = (153*(m + (m > 2 ? -3 : 9)) + 2)/5 + d-1;  // [0, 365]
  const int doe = yoe * 365 + yoe/4 - yoe/100 + doy;         // [0, 146096]
  return era * 146097 + doe - 719468;
}


// Returns year/month/day triple in Gregorian calendar
// Preconditions:
//   z is number of days since 1970-01-01 and is in the range:
//   [numeric_limits<int>::min(), numeric_limits<int>::max()-719468].
//
ymd civil_from_days(int z) noexcept {
  z += 719468;
  const int era = (z >= 0 ? z : z - 146096) / 146097;
  const int doe = z - era * 146097;                                 // [0, 146096]
  const int yoe = (doe - doe/1460 + doe/36524 - doe/146096) / 365;  // [0, 399]
  const int y = yoe + era * 400;
  const int doy = doe - (365*yoe + yoe/4 - yoe/100);                // [0, 365]
  const int mp = (5*doy + 2)/153;                                   // [0, 11]
  const int d = doy - (153*mp+2)/5 + 1;                             // [1, 31]
  const int m = mp + (mp < 10 ? 3 : -9);                            // [1, 12]
  return ymd(y + (m <= 2), m, d);
}


bool is_leap(int y) noexcept {
  return (y % 4 == 0) && (y % 100 != 0 || y % 400 == 0);
}


// Preconditions: m is in [1, 12]
// Returns: The number of days in the month m of common year
// The result is always in the range [28, 31].
//
int last_day_of_month_common_year(int m) noexcept {
  constexpr char a[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  return static_cast<int>(a[m - 1]);
}


// Preconditions: m is in [1, 12]
// Returns: The number of days in the month m of leap year
// The result is always in the range [29, 31].
//
int last_day_of_month_leap_year(int m) noexcept {
  constexpr char a[] = {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  return static_cast<int>(a[m - 1]);
}


// Preconditions: m is in [1, 12]
// Returns: The number of days in the month m of year y
// The result is always in the range [28, 31].
//
int last_day_of_month(int y, int m) noexcept {
  return (m != 2 || !is_leap(y)) ? last_day_of_month_common_year(m) : 29;
}


// Returns day of week in civil calendar
//   iso:    [1 .. 7] <-> [Mon, Tue, Wed, Thu, Fri, Sat, Sun]
//   bible:  [1 .. 7] <-> [Sun, Mon, Tue, Wed, Thu, Fri, Sat]
//
// Preconditions:
//   z is number of days since 1970-01-01 and is in the range:
//   [numeric_limits<int>::min(), numeric_limits<int>::max()-4].
int iso_weekday_from_days(int z) noexcept {
  return (z >= -3) ? (z+3) % 7 + 1 : (z+4) % 7 + 7;
}
int bible_weekday_from_days(int z) noexcept {
  return (z >= -4) ? (z+4) % 7 + 1 : (z+5) % 7 + 7;
}




}  // namespace hh
