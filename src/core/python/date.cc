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

static PyTypeObject* DateTime_TimeZone_Type = nullptr;  // owned
static PyObject* EPOCH_DATETIME = nullptr;  // owned
static PyObject* last_timezone = nullptr;   // owned
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
  last_timezone = datetime_timezone_class.get_attr("utc").release();
  last_timezone_offset = 0;

  EPOCH_DATETIME = oobj::import("datetime", "datetime").call(
      otuple{oint(1970), oint(1), oint(1), oint(0), oint(0), oint(0), oint(0),
             last_timezone}
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
  return PyDate_CheckExact(obj.to_borrowed_ref());
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

static constexpr int64_t NANOSECONDS_PER_MICROSECOND = 1000LL;
static constexpr int64_t NANOSECONDS_PER_SECOND = 1000000000LL;
static constexpr int64_t NANOSECONDS_PER_DAY = 1000000000LL * 24LL * 3600LL;


odatetime::odatetime(PyObject* obj) : oobj(obj) {}  // private

odatetime odatetime::unchecked(PyObject* obj) {
  return odatetime(obj);
}


static_assert(
  ((-1) / 5 == 0) && ((-1LL) / 5LL == 0) && ((-4LL) / 5LL == 0),
  "Expected truncation-towards-zero behavior for integer division of negative numbers");
static_assert(
  ((-1LL + 1) / 5LL - 1 == -1LL) &&
  ((-2LL + 1) / 5LL - 1 == -1LL) &&
  ((-3LL + 1) / 5LL - 1 == -1LL) &&
  ((-4LL + 1) / 5LL - 1 == -1LL) &&
  ((-5LL + 1) / 5LL - 1 == -1LL),
  "Unexpected arithmetic result when dividing negative integers");


odatetime::odatetime(int64_t time) {
  auto days = (time >= 0)? time / NANOSECONDS_PER_DAY
                         : (time + 1) / NANOSECONDS_PER_DAY - 1;
  auto date = hh::civil_from_days(static_cast<int>(days));
  auto time_of_day = time - days * NANOSECONDS_PER_DAY;
  xassert(time_of_day >= 0);
  auto ns = time_of_day % NANOSECONDS_PER_SECOND;
  time_of_day /= NANOSECONDS_PER_SECOND;
  auto microseconds = ns / NANOSECONDS_PER_MICROSECOND;
  auto seconds = time_of_day % 60;
  time_of_day /= 60;
  auto minutes = time_of_day % 60;
  time_of_day /= 60;
  auto hours = time_of_day;
  v = PyDateTime_FromDateAndTime(
      date.year,
      date.month,
      date.day,
      static_cast<int>(hours),
      static_cast<int>(minutes),
      static_cast<int>(seconds),
      static_cast<int>(microseconds)
  );
  if (!v) throw PyError();
}



bool odatetime::check(robj obj) {
  return PyDateTime_CheckExact(obj.to_borrowed_ref());
}


/**
  * This function converts a `datetime.datetime` object into its time offset
  * (in nanoseconds) since the epoch.
  *
  * There are 2 cases to consider here: a "naive" datetime object (without
  * timezone information) gets converted as if it was in UTC. At the same
  * time, a timezone-aware object gets converted taking into account the
  * time zone information. In the latter case, again, we distinguish between
  * time zones that are instances of `datetime.timezone` class, and all others.
  *
  * Specifically, an instance of `datetime.timezone` class has constant offset
  * relative to UTC, which allows us to calculate its time value simpler. In
  * addition, we memorize the last seen timezone object and its offset, so
  * that in the common case when we're processing multiple datetime objects
  * with the same timezeone, we wouldn't have to recalculate the offset.
  */
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
      Py_INCREF(tz);
      Py_SETREF(last_timezone, tz);
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
    // fall-through
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
