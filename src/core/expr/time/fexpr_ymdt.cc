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
#include <algorithm>
#include "_dt.h"
#include "documentation.h"
#include "expr/eval_context.h"
#include "expr/fexpr_func.h"
#include "lib/hh/date.h"
#include "python/xargs.h"
#include "stype.h"
namespace dt {
namespace expr {


//------------------------------------------------------------------------------
// YMDHMS_ColumnImpl
//------------------------------------------------------------------------------

class YMDHMS_ColumnImpl : public Virtual_ColumnImpl {
  private:
    Column year_, month_, day_;
    Column hour_, minute_, second_, ns_;

  public:
    YMDHMS_ColumnImpl(Column&& yr, Column&& mo, Column&& dy, Column&& hr,
                      Column&& mi, Column&& sc, Column&& ns)
      : Virtual_ColumnImpl(yr.nrows(), dt::SType::TIME64),
        year_(std::move(yr)),
        month_(std::move(mo)),
        day_(std::move(dy)),
        hour_(std::move(hr)),
        minute_(std::move(mi)),
        second_(std::move(sc)),
        ns_(std::move(ns))
    {
      xassert(year_.stype() == dt::SType::INT32);
      xassert(month_.stype() == dt::SType::INT32);
      xassert(day_.stype() == dt::SType::INT32);
      xassert(hour_.stype() == dt::SType::INT64);
      xassert(minute_.stype() == dt::SType::INT64);
      xassert(second_.stype() == dt::SType::INT64);
      xassert(ns_.stype() == dt::SType::INT64);
    }

    ColumnImpl* clone() const override {
      return new YMDHMS_ColumnImpl(
          Column(year_), Column(month_), Column(day_), Column(hour_),
          Column(minute_), Column(second_), Column(ns_)
      );
    }

    size_t n_children() const noexcept override {
      return 7;
    }

    const Column& child(size_t i) const override {
      xassert(i <= 6);
      return i == 0? year_ :
             i == 1? month_ :
             i == 2? day_ :
             i == 3? hour_ :
             i == 4? minute_ :
             i == 5? second_ : ns_;
    }

    bool get_element(size_t i, int64_t* out) const override {
      int32_t year, month, day;
      int64_t hour, minute, second, ns;
      bool validY = year_.get_element(i, &year);
      bool validM = month_.get_element(i, &month);
      bool validD = day_.get_element(i, &day);
      bool validH = hour_.get_element(i, &hour);
      bool validW = minute_.get_element(i, &minute);
      bool validS = second_.get_element(i, &second);
      bool validN = ns_.get_element(i, &ns);
      if (validY && validM && validD && validH && validW && validS && validN &&
          (month >= 1 && month <= 12) &&
          (day >= 1 && day <= hh::last_day_of_month(year, month)))
      {
        constexpr int64_t NANOS_PER_SECOND = 1000000000;
        int32_t days = hh::days_from_civil(year, month, day);
        *out = ns + NANOS_PER_SECOND * (
                      second + 60 * (minute + 60 * (hour + 24 * days)));
        return true;
      }
      return false;
    }
};




//------------------------------------------------------------------------------
// DateHMS_ColumnImpl
//------------------------------------------------------------------------------

class DateHMS_ColumnImpl : public Virtual_ColumnImpl {
  private:
    Column date_;
    Column hour_, minute_, second_, ns_;

  public:
    DateHMS_ColumnImpl(Column&& date, Column&& hr, Column&& mi, Column&& sc,
                       Column&& ns)
      : Virtual_ColumnImpl(date.nrows(), dt::SType::TIME64),
        date_(std::move(date)),
        hour_(std::move(hr)),
        minute_(std::move(mi)),
        second_(std::move(sc)),
        ns_(std::move(ns))
    {
      xassert(date_.stype() == dt::SType::DATE32);
      xassert(hour_.stype() == dt::SType::INT64);
      xassert(minute_.stype() == dt::SType::INT64);
      xassert(second_.stype() == dt::SType::INT64);
      xassert(ns_.stype() == dt::SType::INT64);
    }

    ColumnImpl* clone() const override {
      return new DateHMS_ColumnImpl(
          Column(date_), Column(hour_), Column(minute_), Column(second_),
          Column(ns_)
      );
    }

    size_t n_children() const noexcept override {
      return 5;
    }

    const Column& child(size_t i) const override {
      xassert(i <= 4);
      return i == 0? date_ :
             i == 1? hour_ :
             i == 2? minute_ :
             i == 3? second_ : ns_;
    }

    bool get_element(size_t i, int64_t* out) const override {
      int32_t days;
      int64_t hour, minute, second, ns;
      bool validDate = date_.get_element(i, &days);
      bool validHour = hour_.get_element(i, &hour);
      bool validMinute = minute_.get_element(i, &minute);
      bool validSecond = second_.get_element(i, &second);
      bool validNs = ns_.get_element(i, &ns);
      if (validDate && validHour && validMinute && validSecond && validNs) {
        constexpr int64_t NANOS_PER_SECOND = 1000000000;
        *out = ns + NANOS_PER_SECOND * (
                      second + 60 * (minute + 60 * (hour + 24 * days)));
        return true;
      }
      return false;
    }
};




//------------------------------------------------------------------------------
// FExpr_YMDT
//------------------------------------------------------------------------------

class FExpr_YMDT : public FExpr_Func {
  private:
    ptrExpr year_, month_, day_;
    ptrExpr hour_, minute_, second_, ns_;
    ptrExpr date_;

  public:
    FExpr_YMDT(py::robj argYr, py::robj argMo, py::robj argDy,
               py::robj argHr, py::robj argMi, py::robj argSc, py::robj argNs)
      : year_(as_fexpr(argYr)),
        month_(as_fexpr(argMo)),
        day_(as_fexpr(argDy)),
        hour_(as_fexpr(argHr)),
        minute_(as_fexpr(argMi)),
        second_(as_fexpr(argSc)),
        ns_(as_fexpr(argNs))
    {}

    FExpr_YMDT(py::robj argDate,
               py::robj argHr, py::robj argMi, py::robj argSc, py::robj argNs)
      : hour_(as_fexpr(argHr)),
        minute_(as_fexpr(argMi)),
        second_(as_fexpr(argSc)),
        ns_(as_fexpr(argNs)),
        date_(as_fexpr(argDate))
    {}

    std::string repr() const override {
      std::string out = "time.ymdt(";
      if (date_) {
        out += "date=" + date_->repr();
        out += ", hour=" + hour_->repr();
        out += ", minute=" + minute_->repr();
        out += ", second=" + second_->repr();
        out += ", nanosecond=" + ns_->repr();
      } else {
        out += year_->repr();
        out += ", ";
        out += month_->repr();
        out += ", ";
        out += day_->repr();
        out += ", ";
        out += hour_->repr();
        out += ", ";
        out += minute_->repr();
        out += ", ";
        out += second_->repr();
        out += ", ";
        out += ns_->repr();
      }
      out += ")";
      return out;
    }


    Workframe evaluate_n(EvalContext& ctx) const override {
      std::vector<Workframe> wfs;
      if (date_) {
        wfs.push_back(date_->evaluate_n(ctx));
      } else {
        wfs.push_back(year_->evaluate_n(ctx));
        wfs.push_back(month_->evaluate_n(ctx));
        wfs.push_back(day_->evaluate_n(ctx));
      }
      wfs.push_back(hour_->evaluate_n(ctx));
      wfs.push_back(minute_->evaluate_n(ctx));
      wfs.push_back(second_->evaluate_n(ctx));
      wfs.push_back(ns_->evaluate_n(ctx));

      size_t ncols = 1;
      for (size_t i = 0; i < wfs.size(); i++) {
        size_t n = wfs[i].ncols();
        if (ncols == 1 && n > 1) ncols = n;
        if (!(n == ncols || n == 1)) {
          throw InvalidOperationError() << "Incompatible number of columns "
              "for the arguments of `time.ymdt()` function";
        }
      }
      if (ncols > 1) {
        for (size_t i = 0; i < wfs.size(); i++) {
          if (wfs[i].ncols() == 1) wfs[i].repeat_column(ncols);
        }
      }
      auto gmode = Workframe::sync_grouping_mode(wfs);

      Workframe result(ctx);
      for (size_t i = 0; i < ncols; ++i) {
        Column rescol = date_? evaluate1(wfs[0].retrieve_column(i),
                                         wfs[1].retrieve_column(i),
                                         wfs[2].retrieve_column(i),
                                         wfs[3].retrieve_column(i),
                                         wfs[4].retrieve_column(i))
                             : evaluate2(wfs[0].retrieve_column(i),
                                         wfs[1].retrieve_column(i),
                                         wfs[2].retrieve_column(i),
                                         wfs[3].retrieve_column(i),
                                         wfs[4].retrieve_column(i),
                                         wfs[5].retrieve_column(i),
                                         wfs[6].retrieve_column(i));
        result.add_column(std::move(rescol), std::string(), gmode);
      }
      return result;
    }


    static Column evaluate1(
        Column&& dateCol, Column&& hourCol, Column&& minuteCol,
        Column&& secondCol, Column&& nsCol)
    {
      if (dateCol.stype() != dt::SType::DATE32) {
        throw TypeError() << "The date column in function time.ymdt() "
            " should be of type date32, instead it was " << dateCol.type();
      }
      const char* bad = nullptr;
      if (!hourCol.type().is_integer())   bad = "hour";
      if (!minuteCol.type().is_integer()) bad = "minute";
      if (!secondCol.type().is_integer()) bad = "second";
      if (!nsCol.type().is_integer())     bad = "nanosecond";
      if (bad) {
        throw TypeError() << "The " << bad << " column is not integer";
      }
      auto int64 = dt::Type::int64();
      hourCol.cast_inplace(int64);
      minuteCol.cast_inplace(int64);
      secondCol.cast_inplace(int64);
      nsCol.cast_inplace(int64);

      return Column(new DateHMS_ColumnImpl(
          std::move(dateCol), std::move(hourCol), std::move(minuteCol),
          std::move(secondCol), std::move(nsCol))
      );
    }


    static Column evaluate2(
        Column&& yearCol, Column&& monthCol, Column&& dayCol,
        Column&& hourCol, Column&& minuteCol, Column&& secondCol,
        Column&& nsCol)
    {
      const char* bad = nullptr;
      if (!yearCol.type().is_integer())   bad = "year";
      if (!monthCol.type().is_integer())  bad = "month";
      if (!dayCol.type().is_integer())    bad = "day";
      if (!hourCol.type().is_integer())   bad = "hour";
      if (!minuteCol.type().is_integer()) bad = "minute";
      if (!secondCol.type().is_integer()) bad = "second";
      if (!nsCol.type().is_integer())     bad = "nanosecond";
      if (bad) {
        throw TypeError() << "The " << bad << " column is not integer";
      }
      auto int32 = dt::Type::int32();
      auto int64 = dt::Type::int64();
      yearCol.cast_inplace(int32);
      monthCol.cast_inplace(int32);
      dayCol.cast_inplace(int32);
      hourCol.cast_inplace(int64);
      minuteCol.cast_inplace(int64);
      secondCol.cast_inplace(int64);
      nsCol.cast_inplace(int64);

      return Column(new YMDHMS_ColumnImpl(
          std::move(yearCol), std::move(monthCol), std::move(dayCol),
          std::move(hourCol), std::move(minuteCol), std::move(secondCol),
          std::move(nsCol))
      );
    }
};




//------------------------------------------------------------------------------
// Python-facing `ymdt()` function
//------------------------------------------------------------------------------

static py::oobj pyfn_ymdt(const py::XArgs& args) {
  const py::Arg& arg_year       = args[0];
  const py::Arg& arg_month      = args[1];
  const py::Arg& arg_day        = args[2];
  const py::Arg& arg_hour       = args[3];
  const py::Arg& arg_minute     = args[4];
  const py::Arg& arg_second     = args[5];
  const py::Arg& arg_nanosecond = args[6];
  const py::Arg& arg_date       = args[7];
  if (arg_date) {
    if (arg_year || arg_month || arg_day) {
      throw TypeError() << "When argument `date=` is provided, arguments "
          "`year=`, `month=` and `day=` cannot be used.";
    }
    if (!(arg_hour && arg_minute && arg_second)) {
      throw TypeError() << "Function `time.ymdt()` requires four arguments: "
          "date, hour, minute, and second";
    }
    auto date   = arg_date.to_oobj();
    auto hour   = arg_hour.to_oobj();
    auto minute = arg_minute.to_oobj();
    auto second = arg_second.to_oobj();
    auto ns     = arg_nanosecond? arg_nanosecond.to_oobj() : py::oint(0);
    return PyFExpr::make(new FExpr_YMDT(date, hour, minute, second, ns));
  }
  else {
    if (!(arg_year && arg_month && arg_day && arg_hour &&
          arg_minute && arg_second)) {
      throw TypeError() << "Function `time.ymdt()` requires six arguments: "
          "year, month, day, hour, minute, and second";
    }
    auto year   = arg_year.to_oobj();
    auto month  = arg_month.to_oobj();
    auto day    = arg_day.to_oobj();
    auto hour   = arg_hour.to_oobj();
    auto minute = arg_minute.to_oobj();
    auto second = arg_second.to_oobj();
    auto ns     = arg_nanosecond? arg_nanosecond.to_oobj() : py::oint(0);
    return PyFExpr::make(new FExpr_YMDT(year, month, day, hour, minute, second, ns));
  }
}

DECLARE_PYFN(&pyfn_ymdt)
    ->name("ymdt")
    ->docs(doc_time_ymdt)
    ->arg_names({"year", "month", "day", "hour", "minute", "second",
                 "nanosecond", "date"})
    ->n_positional_or_keyword_args(7)
    ->n_keyword_args(1);




}}  // dt::expr
