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



//------------------------------------------------------------------------------
// Individual stats
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



//------------------------------------------------------------------------------
// Frame creation functions
//------------------------------------------------------------------------------

using colmakerfn = Column* (*)(Stats*, const Column*);
static colmakerfn statfns[DT_STYPES_COUNT * NSTATS];
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


oobj Frame::stat(const PKArgs& args) {
  Stat stat = stat_from_args[&args];
  DataTable* res = _make_frame(dt, stat);
  return oobj::from_new_reference(Frame::from_datatable(res));
}



void Frame::Type::_init_stats(Methods& mm) {
  ADD_METHOD(mm, &Frame::stat, args_min);
  ADD_METHOD(mm, &Frame::stat, args_max);
  ADD_METHOD(mm, &Frame::stat, args_mode);
  ADD_METHOD(mm, &Frame::stat, args_sum);
  ADD_METHOD(mm, &Frame::stat, args_mean);
  ADD_METHOD(mm, &Frame::stat, args_sd);

  for (size_t i = 0; i < NSTATS * DT_STYPES_COUNT; ++i) {
    statfns[i] = _nacol;
  }

  // Min
  statfns[id(Stat::Min, SType::BOOL)]    = _mincol_num<int8_t>;
  statfns[id(Stat::Min, SType::INT8)]    = _mincol_num<int8_t>;
  statfns[id(Stat::Min, SType::INT16)]   = _mincol_num<int16_t>;
  statfns[id(Stat::Min, SType::INT32)]   = _mincol_num<int32_t>;
  statfns[id(Stat::Min, SType::INT64)]   = _mincol_num<int64_t>;
  statfns[id(Stat::Min, SType::FLOAT32)] = _mincol_num<float>;
  statfns[id(Stat::Min, SType::FLOAT64)] = _mincol_num<double>;

  // Max
  statfns[id(Stat::Max, SType::BOOL)]    = _maxcol_num<int8_t>;
  statfns[id(Stat::Max, SType::INT8)]    = _maxcol_num<int8_t>;
  statfns[id(Stat::Max, SType::INT16)]   = _maxcol_num<int16_t>;
  statfns[id(Stat::Max, SType::INT32)]   = _maxcol_num<int32_t>;
  statfns[id(Stat::Max, SType::INT64)]   = _maxcol_num<int64_t>;
  statfns[id(Stat::Max, SType::FLOAT32)] = _maxcol_num<float>;
  statfns[id(Stat::Max, SType::FLOAT64)] = _maxcol_num<double>;

  // Mode
  statfns[id(Stat::Mode, SType::BOOL)]    = _modecol_num<int8_t>;
  statfns[id(Stat::Mode, SType::INT8)]    = _modecol_num<int8_t>;
  statfns[id(Stat::Mode, SType::INT16)]   = _modecol_num<int16_t>;
  statfns[id(Stat::Mode, SType::INT32)]   = _modecol_num<int32_t>;
  statfns[id(Stat::Mode, SType::INT64)]   = _modecol_num<int64_t>;
  statfns[id(Stat::Mode, SType::FLOAT32)] = _modecol_num<float>;
  statfns[id(Stat::Mode, SType::FLOAT64)] = _modecol_num<double>;
  statfns[id(Stat::Mode, SType::STR32)]   = _modecol_str<uint32_t>;
  statfns[id(Stat::Mode, SType::STR64)]   = _modecol_str<uint64_t>;

  // Sum
  statfns[id(Stat::Sum, SType::BOOL)]    = _sumcol_num<int8_t>;
  statfns[id(Stat::Sum, SType::INT8)]    = _sumcol_num<int8_t>;
  statfns[id(Stat::Sum, SType::INT16)]   = _sumcol_num<int16_t>;
  statfns[id(Stat::Sum, SType::INT32)]   = _sumcol_num<int32_t>;
  statfns[id(Stat::Sum, SType::INT64)]   = _sumcol_num<int64_t>;
  statfns[id(Stat::Sum, SType::FLOAT32)] = _sumcol_num<float>;
  statfns[id(Stat::Sum, SType::FLOAT64)] = _sumcol_num<double>;

  // Mean
  statfns[id(Stat::Mean, SType::BOOL)]    = _meancol_num<int8_t>;
  statfns[id(Stat::Mean, SType::INT8)]    = _meancol_num<int8_t>;
  statfns[id(Stat::Mean, SType::INT16)]   = _meancol_num<int16_t>;
  statfns[id(Stat::Mean, SType::INT32)]   = _meancol_num<int32_t>;
  statfns[id(Stat::Mean, SType::INT64)]   = _meancol_num<int64_t>;
  statfns[id(Stat::Mean, SType::FLOAT32)] = _meancol_num<float>;
  statfns[id(Stat::Mean, SType::FLOAT64)] = _meancol_num<double>;

  // Sd
  statfns[id(Stat::StDev, SType::BOOL)]    = _sdcol_num<int8_t>;
  statfns[id(Stat::StDev, SType::INT8)]    = _sdcol_num<int8_t>;
  statfns[id(Stat::StDev, SType::INT16)]   = _sdcol_num<int16_t>;
  statfns[id(Stat::StDev, SType::INT32)]   = _sdcol_num<int32_t>;
  statfns[id(Stat::StDev, SType::INT64)]   = _sdcol_num<int64_t>;
  statfns[id(Stat::StDev, SType::FLOAT32)] = _sdcol_num<float>;
  statfns[id(Stat::StDev, SType::FLOAT64)] = _sdcol_num<double>;

  // Args -> Stat map
  stat_from_args[&args_min]  = Stat::Min;
  stat_from_args[&args_max]  = Stat::Max;
  stat_from_args[&args_mode] = Stat::Mode;
  stat_from_args[&args_sum]  = Stat::Sum;
  stat_from_args[&args_mean] = Stat::Mean;
  stat_from_args[&args_sd]   = Stat::StDev;
}


}  // namespace py
