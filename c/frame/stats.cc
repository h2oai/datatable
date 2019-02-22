//------------------------------------------------------------------------------
// Copyright 2018 H2O.ai
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
#include <cstdint>
#include <type_traits>
#include <unordered_map>
#include "frame/py_frame.h"
#include "column.h"
#include "memrange.h"
#include "stats.h"

namespace py {


//------------------------------------------------------------------------------
// Helpers for column creation
//------------------------------------------------------------------------------

template <typename T>
static Column* _make_column(SType stype, T value) {
  Column* col = Column::new_data_column(stype, 1);
  static_cast<FwColumn<T>*>(col)->set_elem(0, value);
  return col;
}

template <typename T>
static Column* _make_column_str(CString value) {
  MemoryRange mbuf = MemoryRange::mem(sizeof(T) * 2);
  MemoryRange strbuf;
  if (value.size >= 0) {
    size_t len = static_cast<size_t>(value.size);
    mbuf.set_element<T>(0, 0);
    mbuf.set_element<T>(1, static_cast<T>(len));
    strbuf.resize(len);
    std::memcpy(strbuf.wptr(), value.ch, len);
  } else {
    mbuf.set_element<T>(0, 0);
    mbuf.set_element<T>(1, GETNA<T>());
  }
  return new StringColumn<T>(1, std::move(mbuf), std::move(strbuf));
}

static Column* _nacol(Stats*, const Column* col) {
  return Column::new_na_column(col->stype(), 1);
}

static oobj _naval(const Column*) {
  return None();
}


//------------------------------------------------------------------------------
// Individual stats (as columns)
//------------------------------------------------------------------------------

template <typename T>
static Column* _mincol_num(Stats* stats, const Column* col) {
  return _make_column(col->stype(),
                      static_cast<NumericalStats<T>*>(stats)->min(col));
}

template <typename T>
static Column* _maxcol_num(Stats* stats, const Column* col) {
  return _make_column(col->stype(),
                      static_cast<NumericalStats<T>*>(stats)->max(col));
}

template <typename T>
static Column* _sumcol_num(Stats* stats, const Column* col) {
  return _make_column(std::is_integral<T>::value? SType::INT64 : SType::FLOAT64,
                      static_cast<NumericalStats<T>*>(stats)->sum(col));
}

template <typename T>
static Column* _meancol_num(Stats* stats, const Column* col) {
  return _make_column(SType::FLOAT64,
                      static_cast<NumericalStats<T>*>(stats)->mean(col));
}

template <typename T>
static Column* _sdcol_num(Stats* stats, const Column* col) {
  return _make_column(SType::FLOAT64,
                      static_cast<NumericalStats<T>*>(stats)->stdev(col));
}

template <typename T>
static Column* _modecol_num(Stats* stats, const Column* col) {
  return _make_column(col->stype(),
                      static_cast<NumericalStats<T>*>(stats)->mode(col));
}

template <typename T>
static Column* _modecol_str(Stats* stats, const Column* col) {
  return _make_column_str<T>(static_cast<StringStats<T>*>(stats)->mode(col));
}

static Column* _countnacol(Stats* stats, const Column* col) {
  return _make_column(SType::INT64,
                      static_cast<int64_t>(stats->countna(col)));
}

static Column* _nuniquecol(Stats* stats, const Column* col) {
  return _make_column(SType::INT64,
                      static_cast<int64_t>(stats->nunique(col)));
}

static Column* _nmodalcol(Stats* stats, const Column* col) {
  return _make_column(SType::INT64,
                      static_cast<int64_t>(stats->nmodal(col)));
}



//------------------------------------------------------------------------------
// Helpers for py values creation
//------------------------------------------------------------------------------

template <SType s> struct Stype2Col {};
template <> struct Stype2Col<SType::BOOL> { const BoolColumn* ptr; };
template <> struct Stype2Col<SType::INT8> { const IntColumn<int8_t>* ptr; };
template <> struct Stype2Col<SType::INT16> { const IntColumn<int16_t>* ptr; };
template <> struct Stype2Col<SType::INT32> { const IntColumn<int32_t>* ptr; };
template <> struct Stype2Col<SType::INT64> { const IntColumn<int64_t>* ptr; };
template <> struct Stype2Col<SType::FLOAT32> { const RealColumn<float>* ptr; };
template <> struct Stype2Col<SType::FLOAT64> { const RealColumn<double>* ptr; };

static oobj pyvalue_bool(void* ptr) {
  int8_t x = *reinterpret_cast<int8_t*>(ptr);
  return ISNA<int8_t>(x)? None() : obool(x);
}

template <typename T>
static oobj pyvalue_int(void* ptr) {
  T x = *reinterpret_cast<T*>(ptr);
  return ISNA<T>(x)? None() : oint(x);
}

template <typename T>
static oobj pyvalue_real(void* ptr) {
  T x = *reinterpret_cast<T*>(ptr);
  return ISNA<T>(x)? None() : ofloat(x);
}

template <SType s> oobj pyvalue(void* ptr);
template <> oobj pyvalue<SType::BOOL>(void* ptr)    { return pyvalue_bool(ptr); }
template <> oobj pyvalue<SType::INT8>(void* ptr)    { return pyvalue_int<int8_t>(ptr); }
template <> oobj pyvalue<SType::INT16>(void* ptr)   { return pyvalue_int<int16_t>(ptr); }
template <> oobj pyvalue<SType::INT32>(void* ptr)   { return pyvalue_int<int32_t>(ptr); }
template <> oobj pyvalue<SType::INT64>(void* ptr)   { return pyvalue_int<int64_t>(ptr); }
template <> oobj pyvalue<SType::FLOAT32>(void* ptr) { return pyvalue_real<float>(ptr); }
template <> oobj pyvalue<SType::FLOAT64>(void* ptr) { return pyvalue_real<double>(ptr); }



//------------------------------------------------------------------------------
// Individual stats (as py::oobj's)
//------------------------------------------------------------------------------

static oobj _countnaval(const Column* col) {
  size_t v = col->countna();
  return pyvalue<SType::INT64>(&v);
}

template <SType stype, SType res>
static oobj _sumval(const Column* col) {
  auto v = static_cast<decltype(Stype2Col<stype>::ptr)>(col)->sum();
  return pyvalue<res>(&v);
}

template <SType stype>
static oobj _meanval(const Column* col) {
  auto v = static_cast<decltype(Stype2Col<stype>::ptr)>(col)->mean();
  return pyvalue<SType::FLOAT64>(&v);
}

// template <typename T>
// static oobj _sdval(const Column* col) {
//   return _make_column(SType::FLOAT64,
//                       static_cast<NumericalStats<T>*>(stats)->stdev(col));
// }




template <SType stype>
static oobj _minval(const Column* col) {
  auto v = static_cast<decltype(Stype2Col<stype>::ptr)>(col)->min();
  return pyvalue<stype>(&v);
}

template <SType stype>
static oobj _maxval(const Column* col) {
  auto v = static_cast<decltype(Stype2Col<stype>::ptr)>(col)->max();
  return pyvalue<stype>(&v);
}

// template <typename T>
// static oobj _modecol_num(Stats* stats, const Column* col) {
//   return _make_column(col->stype(),
//                       static_cast<NumericalStats<T>*>(stats)->mode(col));
// }

// template <typename T>
// static oobj _modecol_str(Stats* stats, const Column* col) {
//   return _make_column_str<T>(static_cast<StringStats<T>*>(stats)->mode(col));
// }

// static oobj _nuniquecol(Stats* stats, const Column* col) {
//   return _make_column(SType::INT64,
//                       static_cast<int64_t>(stats->nunique(col)));
// }

// static oobj _nmodalcol(Stats* stats, const Column* col) {
//   return _make_column(SType::INT64,
//                       static_cast<int64_t>(stats->nmodal(col)));
// }




//------------------------------------------------------------------------------
// Frame creation functions
//------------------------------------------------------------------------------

using colmakerfn = Column* (*)(Stats*, const Column*);
using colmakerfn1 = oobj (*)(const Column*);
static colmakerfn statfns[DT_STYPES_COUNT * NSTATS];
static colmakerfn1 statfns1[DT_STYPES_COUNT * NSTATS];
static std::unordered_map<const PKArgs*, Stat> stat_from_args;

inline constexpr size_t id(Stat stat, SType stype) {
  return static_cast<size_t>(stype) * NSTATS + static_cast<size_t>(stat);
}

static DataTable* _make_frame(DataTable* dt, Stat stat) {
  colvec out_cols;
  out_cols.reserve(dt->ncols);
  for (Column* dtcol : dt->columns) {
    SType stype = dtcol->stype();
    colmakerfn f = statfns[id(stat, stype)];
    Column* newcol = f(dtcol->get_stats(), dtcol);
    out_cols.push_back(newcol);
  }
  return new DataTable(std::move(out_cols), dt);
}



//------------------------------------------------------------------------------
// Frame functions
//------------------------------------------------------------------------------

static PKArgs args_min(0, 0, 0, false, false, {}, "min", nullptr);
static PKArgs args_max(0, 0, 0, false, false, {}, "max", nullptr);
static PKArgs args_mode(0, 0, 0, false, false, {}, "mode", nullptr);
static PKArgs args_sum(0, 0, 0, false, false, {}, "sum", nullptr);
static PKArgs args_mean(0, 0, 0, false, false, {}, "mean", nullptr);
static PKArgs args_sd(0, 0, 0, false, false, {}, "sd", nullptr);
static PKArgs args_countna(0, 0, 0, false, false, {}, "countna", nullptr);
static PKArgs args_nunique(0, 0, 0, false, false, {}, "nunique", nullptr);
static PKArgs args_nmodal(0, 0, 0, false, false, {}, "nmodal", nullptr);

oobj Frame::stat(const PKArgs& args) {
  Stat stat = stat_from_args[&args];
  DataTable* res = _make_frame(dt, stat);
  return oobj::from_new_reference(Frame::from_datatable(res));
}


static PKArgs args_countna1(0, 0, 0, false, false, {}, "countna1", nullptr);
static PKArgs args_sum1(0, 0, 0, false, false, {}, "sum1", nullptr);
static PKArgs args_mean1(0, 0, 0, false, false, {}, "mean1", nullptr);
static PKArgs args_min1(0, 0, 0, false, false, {}, "min1", nullptr);
static PKArgs args_max1(0, 0, 0, false, false, {}, "max1", nullptr);

oobj Frame::stat1(const PKArgs& args) {
  if (dt->ncols != 1) {
    throw ValueError() << "This method can only be applied to a 1-column Frame";
  }
  Column* col0 = dt->columns[0];
  Stat stat = stat_from_args[&args];
  SType stype = col0->stype();
  colmakerfn1 f = statfns1[id(stat, stype)];
  return f(col0);
}



void Frame::Type::_init_stats(Methods& mm) {
  ADD_METHOD(mm, &Frame::stat, args_countna);
  ADD_METHOD(mm, &Frame::stat, args_sum);
  ADD_METHOD(mm, &Frame::stat, args_min);
  ADD_METHOD(mm, &Frame::stat, args_max);
  ADD_METHOD(mm, &Frame::stat, args_mode);
  ADD_METHOD(mm, &Frame::stat, args_mean);
  ADD_METHOD(mm, &Frame::stat, args_sd);
  ADD_METHOD(mm, &Frame::stat, args_nunique);
  ADD_METHOD(mm, &Frame::stat, args_nmodal);

  ADD_METHOD(mm, &Frame::stat1, args_countna1);
  ADD_METHOD(mm, &Frame::stat1, args_sum1);
  ADD_METHOD(mm, &Frame::stat1, args_mean1);
  ADD_METHOD(mm, &Frame::stat1, args_min1);
  ADD_METHOD(mm, &Frame::stat1, args_max1);

  for (size_t i = 0; i < NSTATS * DT_STYPES_COUNT; ++i) {
    statfns[i] = _nacol;
    statfns1[i] = _naval;
  }

  //---- Column statfns --------------------------------------------------------

  // Stat::NaCount (= 0)
  statfns[id(Stat::NaCount, SType::BOOL)]    = _countnacol;
  statfns[id(Stat::NaCount, SType::INT8)]    = _countnacol;
  statfns[id(Stat::NaCount, SType::INT16)]   = _countnacol;
  statfns[id(Stat::NaCount, SType::INT32)]   = _countnacol;
  statfns[id(Stat::NaCount, SType::INT64)]   = _countnacol;
  statfns[id(Stat::NaCount, SType::FLOAT32)] = _countnacol;
  statfns[id(Stat::NaCount, SType::FLOAT64)] = _countnacol;
  statfns[id(Stat::NaCount, SType::STR32)]   = _countnacol;
  statfns[id(Stat::NaCount, SType::STR64)]   = _countnacol;
  statfns[id(Stat::NaCount, SType::OBJ)]     = _countnacol;

  // Stat::Sum (= 1)
  statfns[id(Stat::Sum, SType::BOOL)]    = _sumcol_num<int8_t>;
  statfns[id(Stat::Sum, SType::INT8)]    = _sumcol_num<int8_t>;
  statfns[id(Stat::Sum, SType::INT16)]   = _sumcol_num<int16_t>;
  statfns[id(Stat::Sum, SType::INT32)]   = _sumcol_num<int32_t>;
  statfns[id(Stat::Sum, SType::INT64)]   = _sumcol_num<int64_t>;
  statfns[id(Stat::Sum, SType::FLOAT32)] = _sumcol_num<float>;
  statfns[id(Stat::Sum, SType::FLOAT64)] = _sumcol_num<double>;

  // Stat::Mean (= 2)
  statfns[id(Stat::Mean, SType::BOOL)]    = _meancol_num<int8_t>;
  statfns[id(Stat::Mean, SType::INT8)]    = _meancol_num<int8_t>;
  statfns[id(Stat::Mean, SType::INT16)]   = _meancol_num<int16_t>;
  statfns[id(Stat::Mean, SType::INT32)]   = _meancol_num<int32_t>;
  statfns[id(Stat::Mean, SType::INT64)]   = _meancol_num<int64_t>;
  statfns[id(Stat::Mean, SType::FLOAT32)] = _meancol_num<float>;
  statfns[id(Stat::Mean, SType::FLOAT64)] = _meancol_num<double>;

  // Stat::StDev (= 3)
  statfns[id(Stat::StDev, SType::BOOL)]    = _sdcol_num<int8_t>;
  statfns[id(Stat::StDev, SType::INT8)]    = _sdcol_num<int8_t>;
  statfns[id(Stat::StDev, SType::INT16)]   = _sdcol_num<int16_t>;
  statfns[id(Stat::StDev, SType::INT32)]   = _sdcol_num<int32_t>;
  statfns[id(Stat::StDev, SType::INT64)]   = _sdcol_num<int64_t>;
  statfns[id(Stat::StDev, SType::FLOAT32)] = _sdcol_num<float>;
  statfns[id(Stat::StDev, SType::FLOAT64)] = _sdcol_num<double>;

  // Stat::Min (= 6)
  statfns[id(Stat::Min, SType::BOOL)]    = _mincol_num<int8_t>;
  statfns[id(Stat::Min, SType::INT8)]    = _mincol_num<int8_t>;
  statfns[id(Stat::Min, SType::INT16)]   = _mincol_num<int16_t>;
  statfns[id(Stat::Min, SType::INT32)]   = _mincol_num<int32_t>;
  statfns[id(Stat::Min, SType::INT64)]   = _mincol_num<int64_t>;
  statfns[id(Stat::Min, SType::FLOAT32)] = _mincol_num<float>;
  statfns[id(Stat::Min, SType::FLOAT64)] = _mincol_num<double>;

  // Stat::Max (= 10)
  statfns[id(Stat::Max, SType::BOOL)]    = _maxcol_num<int8_t>;
  statfns[id(Stat::Max, SType::INT8)]    = _maxcol_num<int8_t>;
  statfns[id(Stat::Max, SType::INT16)]   = _maxcol_num<int16_t>;
  statfns[id(Stat::Max, SType::INT32)]   = _maxcol_num<int32_t>;
  statfns[id(Stat::Max, SType::INT64)]   = _maxcol_num<int64_t>;
  statfns[id(Stat::Max, SType::FLOAT32)] = _maxcol_num<float>;
  statfns[id(Stat::Max, SType::FLOAT64)] = _maxcol_num<double>;

  // Stat::Mode (= 11)
  statfns[id(Stat::Mode, SType::BOOL)]    = _modecol_num<int8_t>;
  statfns[id(Stat::Mode, SType::INT8)]    = _modecol_num<int8_t>;
  statfns[id(Stat::Mode, SType::INT16)]   = _modecol_num<int16_t>;
  statfns[id(Stat::Mode, SType::INT32)]   = _modecol_num<int32_t>;
  statfns[id(Stat::Mode, SType::INT64)]   = _modecol_num<int64_t>;
  statfns[id(Stat::Mode, SType::FLOAT32)] = _modecol_num<float>;
  statfns[id(Stat::Mode, SType::FLOAT64)] = _modecol_num<double>;
  statfns[id(Stat::Mode, SType::STR32)]   = _modecol_str<uint32_t>;
  statfns[id(Stat::Mode, SType::STR64)]   = _modecol_str<uint64_t>;

  // Stat::NModal (= 12)
  statfns[id(Stat::NModal, SType::BOOL)]    = _nmodalcol;
  statfns[id(Stat::NModal, SType::INT8)]    = _nmodalcol;
  statfns[id(Stat::NModal, SType::INT16)]   = _nmodalcol;
  statfns[id(Stat::NModal, SType::INT32)]   = _nmodalcol;
  statfns[id(Stat::NModal, SType::INT64)]   = _nmodalcol;
  statfns[id(Stat::NModal, SType::FLOAT32)] = _nmodalcol;
  statfns[id(Stat::NModal, SType::FLOAT64)] = _nmodalcol;
  statfns[id(Stat::NModal, SType::STR32)]   = _nmodalcol;
  statfns[id(Stat::NModal, SType::STR64)]   = _nmodalcol;

  // Stat::NUnique (= 13)
  statfns[id(Stat::NUnique, SType::BOOL)]    = _nuniquecol;
  statfns[id(Stat::NUnique, SType::INT8)]    = _nuniquecol;
  statfns[id(Stat::NUnique, SType::INT16)]   = _nuniquecol;
  statfns[id(Stat::NUnique, SType::INT32)]   = _nuniquecol;
  statfns[id(Stat::NUnique, SType::INT64)]   = _nuniquecol;
  statfns[id(Stat::NUnique, SType::FLOAT32)] = _nuniquecol;
  statfns[id(Stat::NUnique, SType::FLOAT64)] = _nuniquecol;
  statfns[id(Stat::NUnique, SType::STR32)]   = _nuniquecol;
  statfns[id(Stat::NUnique, SType::STR64)]   = _nuniquecol;


  //---- Scalar statfns --------------------------------------------------------

  // Stat::NaCount (= 0)
  statfns1[id(Stat::NaCount, SType::BOOL)]    = _countnaval;
  statfns1[id(Stat::NaCount, SType::INT8)]    = _countnaval;
  statfns1[id(Stat::NaCount, SType::INT16)]   = _countnaval;
  statfns1[id(Stat::NaCount, SType::INT32)]   = _countnaval;
  statfns1[id(Stat::NaCount, SType::INT64)]   = _countnaval;
  statfns1[id(Stat::NaCount, SType::FLOAT32)] = _countnaval;
  statfns1[id(Stat::NaCount, SType::FLOAT64)] = _countnaval;
  statfns1[id(Stat::NaCount, SType::STR32)]   = _countnaval;
  statfns1[id(Stat::NaCount, SType::STR64)]   = _countnaval;
  statfns1[id(Stat::NaCount, SType::OBJ)]     = _countnaval;

  // Stat::Sum (= 1)
  statfns1[id(Stat::Sum, SType::BOOL)]    = _sumval<SType::BOOL,  SType::INT64>;
  statfns1[id(Stat::Sum, SType::INT8)]    = _sumval<SType::INT8,  SType::INT64>;
  statfns1[id(Stat::Sum, SType::INT16)]   = _sumval<SType::INT16, SType::INT64>;
  statfns1[id(Stat::Sum, SType::INT32)]   = _sumval<SType::INT32, SType::INT64>;
  statfns1[id(Stat::Sum, SType::INT64)]   = _sumval<SType::INT64, SType::INT64>;
  statfns1[id(Stat::Sum, SType::FLOAT32)] = _sumval<SType::FLOAT32, SType::FLOAT64>;
  statfns1[id(Stat::Sum, SType::FLOAT64)] = _sumval<SType::FLOAT64, SType::FLOAT64>;

  // Stat::Mean (= 2)
  statfns1[id(Stat::Mean, SType::BOOL)]    = _meanval<SType::BOOL>;
  statfns1[id(Stat::Mean, SType::INT8)]    = _meanval<SType::INT8>;
  statfns1[id(Stat::Mean, SType::INT16)]   = _meanval<SType::INT16>;
  statfns1[id(Stat::Mean, SType::INT32)]   = _meanval<SType::INT32>;
  statfns1[id(Stat::Mean, SType::INT64)]   = _meanval<SType::INT64>;
  statfns1[id(Stat::Mean, SType::FLOAT32)] = _meanval<SType::FLOAT32>;
  statfns1[id(Stat::Mean, SType::FLOAT64)] = _meanval<SType::FLOAT64>;

  // Stat::Min (= 6)
  statfns1[id(Stat::Min, SType::BOOL)]    = _minval<SType::BOOL>;
  statfns1[id(Stat::Min, SType::INT8)]    = _minval<SType::INT8>;
  statfns1[id(Stat::Min, SType::INT16)]   = _minval<SType::INT16>;
  statfns1[id(Stat::Min, SType::INT32)]   = _minval<SType::INT32>;
  statfns1[id(Stat::Min, SType::INT64)]   = _minval<SType::INT64>;
  statfns1[id(Stat::Min, SType::FLOAT32)] = _minval<SType::FLOAT32>;
  statfns1[id(Stat::Min, SType::FLOAT64)] = _minval<SType::FLOAT64>;

  // Stat::Max (= 10)
  statfns1[id(Stat::Max, SType::BOOL)]    = _maxval<SType::BOOL>;
  statfns1[id(Stat::Max, SType::INT8)]    = _maxval<SType::INT8>;
  statfns1[id(Stat::Max, SType::INT16)]   = _maxval<SType::INT16>;
  statfns1[id(Stat::Max, SType::INT32)]   = _maxval<SType::INT32>;
  statfns1[id(Stat::Max, SType::INT64)]   = _maxval<SType::INT64>;
  statfns1[id(Stat::Max, SType::FLOAT32)] = _maxval<SType::FLOAT32>;
  statfns1[id(Stat::Max, SType::FLOAT64)] = _maxval<SType::FLOAT64>;


  //---- Args -> Stat map ------------------------------------------------------

  stat_from_args[&args_countna] = Stat::NaCount;
  stat_from_args[&args_sum]     = Stat::Sum;
  stat_from_args[&args_mean]    = Stat::Mean;
  stat_from_args[&args_sd]      = Stat::StDev;
  stat_from_args[&args_min]     = Stat::Min;
  stat_from_args[&args_max]     = Stat::Max;
  stat_from_args[&args_mode]    = Stat::Mode;
  stat_from_args[&args_nmodal]  = Stat::NModal;
  stat_from_args[&args_nunique] = Stat::NUnique;

  stat_from_args[&args_countna1] = Stat::NaCount;
  stat_from_args[&args_sum1]     = Stat::Sum;
  stat_from_args[&args_mean1]    = Stat::Mean;
  stat_from_args[&args_min1]     = Stat::Min;
  stat_from_args[&args_max1]     = Stat::Max;
}


}  // namespace py
