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
#ifndef hh_DATE_h
#define hh_DATE_h
namespace hh {


struct ymd {
  int year;
  int month;
  int day;
  int : 32;

  constexpr ymd(int y, int m, int d) : year(y), month(m), day(d) {}
};


int days_from_civil(int y, int m, int d) noexcept;
ymd civil_from_days(int z) noexcept;
bool is_leap(int y) noexcept;
int last_day_of_month_common_year(int m) noexcept;
int last_day_of_month_leap_year(int m) noexcept;
int last_day_of_month(int y, int m) noexcept;
int iso_weekday_from_days(int z) noexcept;
int bible_weekday_from_days(int z) noexcept;



}  // namespace hh
#endif
