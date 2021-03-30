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
#include <Python.h>
#include <datetime.h>     // Python "datetime" module, not included in Python.h
#include "python/date.h"
#include "python/datetime.h"
#include "python/int.h"
#include "python/tuple.h"
#include "lib/hh/date.h"
#include "utils/assert.h"
namespace py {

static PyTypeObject* DateTime_TimeZone_Type = nullptr;
static PyObject* EPOCH_DATETIME = nullptr;
static PyObject* last_timezone = nullptr;
static int64_t last_timezone_offset = 0;

#define PyDateTime_DATE_HAS_TIMEZONE(o) \
    ((reinterpret_cast<PyDateTime_DateTime*>(o))->hastzinfo)

#define PyDateTime_DATE_GET_TIMEZONE(o) \
    ((reinterpret_cast<PyDateTime_DateTime*>(o))->tzinfo)


// In order to be able to use python API to access datetime objects,
// we need to "import" it via a special macro. This macro loads the
// datetime module as a capsule and stores it in PyDateTimeAPI variable.
//
// Note that the PyDateTimeAPI variable will be local to this file,
// which means that no PyDateTime* macros will work outside of this
// translation unit.
//
// See https://docs.python.org/3/c-api/datetime.html
void datetime_init() {
  PyDateTime_IMPORT;

  auto datetime_timezone_class = oobj::import("datetime", "timezone");
  EPOCH_DATETIME = oobj::import("datetime", "datetime").call(
      otuple{oint(1970), oint(1), oint(1), oint(0), oint(0), oint(0), oint(0),
             datetime_timezone_class.get_attr("utc")}
  ).release();
  DateTime_TimeZone_Type = reinterpret_cast<PyTypeObject*>(
      std::move(datetime_timezone_class).release());
}



//------------------------------------------------------------------------------
// odate
//------------------------------------------------------------------------------

odate::odate(PyObject* obj) : oobj(obj) {}  // private

odate odate::unchecked(PyObject* obj) {
  return odate(obj);
}


odate::odate(hh::ymd date) {
  if (date.year > 0 && date.year <= 9999) {
    v = PyDate_FromDate(date.year, date.month, date.day);
  } else {
    v = PyLong_FromLong(hh::days_from_civil(date.year, date.month, date.day));
  }
  if (!v) throw PyError();
}

odate::odate(int32_t days)
  : odate(hh::civil_from_days(days)) {}



bool odate::check(robj obj) {
  return PyDate_Check(obj.to_borrowed_ref());
}


int odate::get_days() const {
  return hh::days_from_civil(
      PyDateTime_GET_YEAR(v),
      PyDateTime_GET_MONTH(v),
      PyDateTime_GET_DAY(v)
  );
}


PyTypeObject* odate::type() {
  return PyDateTimeAPI->DateType;
}




//------------------------------------------------------------------------------
// odatetime
//------------------------------------------------------------------------------

static constexpr int64_t NANOSECONDS_PER_MICROSECOND = 1000L;
static constexpr int64_t NANOSECONDS_PER_SECOND = 1000000000L;
static constexpr int64_t NANOSECONDS_PER_DAY = 1000000000L * 24L * 3600L;


odatetime::odatetime(PyObject* obj) : oobj(obj) {}  // private

odatetime odatetime::unchecked(PyObject* obj) {
  return odatetime(obj);
}


odatetime::odatetime(int64_t time) {
  const int days = static_cast<int>(
      (time >= 0? time : time - NANOSECONDS_PER_DAY + 1) / NANOSECONDS_PER_DAY);
  const hh::ymd date = hh::civil_from_days(days);
  int64_t time_of_day = time - days * NANOSECONDS_PER_DAY;
  xassert(time_of_day >= 0);
  int64_t ns = time_of_day % NANOSECONDS_PER_DAY;
  time_of_day /= NANOSECONDS_PER_DAY;
  const int microseconds = static_cast<int>(ns / NANOSECONDS_PER_MICROSECOND);
  const int seconds = static_cast<int>(time_of_day / 60);
  time_of_day /= 60;
  const int minutes = static_cast<int>(time_of_day / 60);
  time_of_day /= 60;
  const int hours = static_cast<int>(time_of_day);
  v = PyDateTime_FromDateAndTime(date.year, date.month, date.day,
      hours, minutes, seconds, microseconds);
  if (!v) throw PyError();
}



bool odatetime::check(robj obj) {
  return PyDateTime_Check(obj.to_borrowed_ref());
}


int64_t odatetime::get_time() const {
  int64_t offset = 0;
  if (PyDateTime_DATE_HAS_TIMEZONE(v)) {
    PyObject* tz = PyDateTime_DATE_GET_TIMEZONE(v);  // borrowed ref
    if (tz == last_timezone) {
      offset = last_timezone_offset;
    }
    else if (tz->ob_type == DateTime_TimeZone_Type) {
      double delta = robj(tz).invoke("utcofffset", {None()})
          .invoke("total_seconds").to_double();
      offset = static_cast<int64_t>(delta) * NANOSECONDS_PER_SECOND;
      last_timezone_offset = offset;
      last_timezone = tz;
    }
    else {
      auto delta = this->invoke("__sub__", oobj(EPOCH_DATETIME));
      auto deltav = delta.to_borrowed_ref();
      int days = PyDateTime_DELTA_GET_DAYS(deltav);
      int seconds = PyDateTime_DELTA_GET_SECONDS(deltav);
      int micros = PyDateTime_DELTA_GET_MICROSECONDS(deltav);
      return NANOSECONDS_PER_DAY * days +
             NANOSECONDS_PER_SECOND * seconds +
             NANOSECONDS_PER_MICROSECOND * micros;
    }
  }
  int days = hh::days_from_civil(
      PyDateTime_GET_YEAR(v),
      PyDateTime_GET_MONTH(v),
      PyDateTime_GET_DAY(v)
  );
  int hours = PyDateTime_DATE_GET_HOUR(v);
  int minutes = PyDateTime_DATE_GET_MINUTE(v);
  int seconds = PyDateTime_DATE_GET_SECOND(v);
  int micros = PyDateTime_DATE_GET_MICROSECOND(v);
  return NANOSECONDS_PER_DAY * days +
         NANOSECONDS_PER_SECOND * ((hours*60 + minutes)*60 + seconds) +
         NANOSECONDS_PER_MICROSECOND * micros +
         offset;
}


PyTypeObject* odatetime::type() {
  return PyDateTimeAPI->DateTimeType;
}




}  // namespace py
