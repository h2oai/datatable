//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018-2019
//------------------------------------------------------------------------------
#include <atomic>       // std::atomic
#include <cmath>        // std::isinf, std::sqrt
#include <limits>       // std::numeric_limits
#include <type_traits>  // std::is_floating_point
#include "lib/parallel_hashmap/phmap.h"
#include "models/murmurhash.h"
#include "parallel/api.h"
#include "python/_all.h"
#include "python/string.h"
#include "utils/misc.h"
#include "column.h"
#include "datatablemodule.h"
#include "rowindex.h"
#include "stats.h"


template<typename T>
constexpr T infinity() {
  return std::numeric_limits<T>::has_infinity
         ? std::numeric_limits<T>::infinity()
         : std::numeric_limits<T>::max();
}


static const char* stat_name(Stat s) {
  switch (s) {
    case Stat::NaCount: return "NaCount";
    case Stat::Sum:     return "Sum";
    case Stat::Mean:    return "Mean";
    case Stat::StDev:   return "StDev";
    case Stat::Skew:    return "Skew";
    case Stat::Kurt:    return "Kurt";
    case Stat::Min:     return "Min";
    case Stat::Qt25:    return "Qt25";
    case Stat::Median:  return "Median";
    case Stat::Qt75:    return "Qt75";
    case Stat::Max:     return "Max";
    case Stat::Mode:    return "Mode";
    case Stat::NModal:  return "NModal";
    case Stat::NUnique: return "NUnique";
  }
  throw RuntimeError() << "Unknown stat " << int(s);
}



//------------------------------------------------------------------------------
// main Stats class
//------------------------------------------------------------------------------

Stats::Stats(const OColumn& col) : column(col) {}

Stats::~Stats() {}


void Stats::reset() {
  _computed.reset();
  _valid.reset();
}

bool Stats::is_computed(Stat stat) const {
  return _computed.test(static_cast<size_t>(stat));
}

bool Stats::is_valid(Stat stat) const {
  return _valid.test(static_cast<size_t>(stat));
}

void Stats::set_computed(Stat stat, bool flag) {
  _computed.set(static_cast<size_t>(stat), flag);
}

void Stats::set_valid(Stat stat, bool flag) {
  _valid.set(static_cast<size_t>(stat), flag);
}




//------------------------------------------------------------------------------
// Stats getters (generic)
//------------------------------------------------------------------------------

static int64_t _wrap_int_stat(size_t value, bool* isvalid) {
  if (isvalid) *isvalid = true;
  return static_cast<int64_t>(value);
}

int64_t Stats::get_stat_int(Stat stat, bool* isvalid) {
  switch (stat) {
    case Stat::Min:     return min_int(isvalid);
    case Stat::Max:     return max_int(isvalid);
    case Stat::Mode:    return mode_int(isvalid);
    case Stat::NaCount: return _wrap_int_stat(nacount(), isvalid);
    case Stat::NUnique: return _wrap_int_stat(nunique(), isvalid);
    case Stat::NModal:  return _wrap_int_stat(nmodal(), isvalid);
    default:            return false;
  }
}


double Stats::get_stat_dbl(Stat stat, bool* isvalid) {
  switch (stat) {
    case Stat::Sum: {
      if (isvalid) *isvalid = true;
      return sum();
    }
    case Stat::Mean:  return mean(isvalid);
    case Stat::StDev: return stdev(isvalid);
    case Stat::Skew:  return skew(isvalid);
    case Stat::Kurt:  return jurt(isvalid);
    case Stat::Min:   return min_dbl(isvalid);
    case Stat::Max:   return max_dbl(isvalid);
    case Stat::Mode:  return mode_dbl(isvalid);
    default:          return false;
  }
}


CString Stats::get_stat_str(Stat stat, bool* isvalid) {
  switch (stat) {
    case Stat::Mode: return mode_str(isvalid);
    default:         return false;
  }
}


bool Stats::get_stat(Stat stat, int64_t* out) {
  bool ret;
  *out = get_stat_int(stat, &ret);
  return ret;
}

bool Stats::get_stat(Stat stat, double* out)   {
  bool ret;
  *out = get_stat_dbl(stat, &ret);
  return ret;
}

bool Stats::get_stat(Stat stat, CString* out)  {
  bool ret;
  *out = get_stat_str(stat, &ret);
  return ret;
}




//------------------------------------------------------------------------------
// Stats getters (specific)
//------------------------------------------------------------------------------

size_t Stats::nacount() {
  if (!is_computed(Stat::NaCount)) compute_countna();
  return _countna;
}


size_t Stats::nunique() { return 0; }
size_t Stats::nmodal () { return 0; }
double Stats::sum(bool* isvalid) { return 0; }
double Stats::mean(bool* isvalid) { return 0; }
double Stats::stdev(bool* isvalid) { return 0; }
double Stats::skew(bool* isvalid) { return 0; }
double Stats::kurt(bool* isvalid) { return 0; }
int64_t Stats::min(bool* isvalid) { return 0; }
double Stats::min(bool* isvalid) { return 0; }
int64_t Stats::max(bool* isvalid) { return 0; }
double Stats::max(bool* isvalid) { return 0; }
int64_t Stats::mode(bool* isvalid) { return 0; }
double Stats::mode(bool* isvalid) { return 0; }
CString Stats::mode(bool* isvalid) { return CString(); }




//------------------------------------------------------------------------------
// Stats computation
//------------------------------------------------------------------------------

template <typename T>
static size_t _compute_countna(const OColumn& col) {
  std::atomic<size_t> total_countna = 0;
  dt::parallel_region(
    [&] {
      T target;
      size_t thread_countna = 0;
      dt::nested_for_static(col.nrows(),
        [&](size_t i) {
          bool isna = col.get_element(i, &target);
          thread_countna += isna;
        });
      total_countna += thread_countna;
    });
  return total_countna.load();
}

void Stats::compute_countna() {
  switch (column.stype()) {
    case SType::BOOL:
    case SType::INT8:
    case SType::INT16:
    case SType::INT32:   _countna = _compute_countna<int32_t>(column); break;
    case SType::INT64:   _countna = _compute_countna<int64_t>(column); break;
    case SType::FLOAT32: _countna = _compute_countna<float>(column); break;
    case SType::FLOAT64: _countna = _compute_countna<double>(column); break;
    case SType::STR32:
    case SType::STR64:   _countna = _compute_countna<CString>(column); break;
    case SType::OBJ:     _countna = _compute_countna<py::robj>(column); break;
    default: throw NotImplError();
  }
  set_computed(Stat::NaCount, true);
  set_valid(Stat::NaCount, true);
}








void Stats::compute_nunique(const Column* col) {
  compute_sorted_stats(col);
}



size_t Stats::nunique(const Column* col) {
  if (!is_computed(Stat::NUnique)) compute_nunique(col);
  return _nunique;
}

size_t Stats::nmodal(const Column* col) {
  if (!is_computed(Stat::NModal)) compute_sorted_stats(col);
  return _nmodal;
}

size_t Stats::memory_footprint() const {
  return sizeof(*this);
}


void Stats::merge_stats(const Stats*) {
  // TODO: implement
}


/**
 * See DataTable::verify_integrity for method description
 */
// void Stats::verify_integrity(const Column* col) const {
//   auto test = std::unique_ptr<Stats>(make());
//   verify_stat(Stat::NaCount, _countna, [&](){ return test->countna(col); });
//   verify_stat(Stat::NUnique, _nunique, [&](){ return test->nunique(col); });
//   verify_stat(Stat::NModal,  _nmodal,  [&](){ return test->nmodal(col); });
//   verify_more(test.get(), col);
// }

// void Stats::verify_more(Stats*, const Column*) const {}


// template <typename T, typename F>
// void Stats::verify_stat(Stat s, T value, F getter) const {
//   if (!is_computed(s)) return;
//   T test_value = getter();
//   if (value == test_value) return;
//   if (std::is_floating_point<T>::value && (value != 0) &&
//       std::abs(1.0 - static_cast<double>(test_value/value)) < 1e-12) return;
//   throw AssertionError()
//       << "Stored " << stat_name(s) << " stat is " << value
//       << ", whereas computed " << stat_name(s) << " is " << getter();
// }



//==============================================================================
// NumericalStats
//==============================================================================

/**
 * Standard deviation and mean computations are done using Welford's method.
 * Ditto for skewness and kurtosis computations.
 * (Source: https://www.johndcook.com/blog/standard_deviation)
 */
template <typename T>
void NumericStats<T>::compute_numerical_stats(const Column* col) {
  size_t nrows = col->nrows;
  size_t count_notna = 0;
  double mean = 0;
  double m2 = 0;
  double m3 = 0;
  double m4 = 0;
  double sum = 0;
  T min = infinity<T>();
  T max = -infinity<T>();
  std::mutex mutex;

  dt::parallel_region(
    [&] {
      size_t t_count_notna = 0;
      size_t n1 = 0;
      size_t n2 = 0; // added for readability
      double t_mean = 0;
      double t_m2 = 0;
      double t_m3 = 0;
      double t_m4 = 0;
      double t_m2_helper = 0;

      double t_sum = 0;
      T t_min = infinity<T>();
      T t_max = -infinity<T>();

      dt::nested_for_static(nrows,
        [&](size_t i) {
          T x;
          bool r = col->get_element(i, &x);
          if (r) return;
          n1 = t_count_notna;
          ++t_count_notna;
          n2 = t_count_notna; // readability
          t_sum += static_cast<double>(x);
          if (x < t_min) t_min = x;  // Note: these ifs are not exclusive!
          if (x > t_max) t_max = x;
          double delta = static_cast<double>(x) - t_mean;
          double delta_n = static_cast<double>(delta) / t_count_notna;
          double delta_n2 = delta_n * delta_n;
          double term1 = delta * delta_n * static_cast<double>(n1);
          t_mean += delta / t_count_notna;
          double delta2 = static_cast<double>(x) - t_mean;
          t_m4 += term1 * delta_n2 * (n2 * n2 - 3 * n2 + 3);
          t_m4 += 6 * delta_n2 * t_m2_helper - 4 * delta_n * t_m3;
          t_m3 += (term1 * delta_n * (n2 - 2) - 3 * delta_n * t_m2_helper);
          t_m2 += delta * delta2;
          t_m2_helper += term1;
        });

      if (t_count_notna) {
        std::lock_guard<std::mutex> lock(mutex);
        size_t nold = count_notna;
        count_notna += t_count_notna;
        sum += t_sum;
        if (t_min < min) min = t_min;
        if (t_max > max) max = t_max;
        double delta = mean - t_mean;
        double delta2 = delta * delta;
        double delta3 = delta2 * delta;
        double delta4 = delta2 * delta2;
        double a_m2 = m2;
        double a_m3 = m3;
        double b_m2 = t_m2;
        double b_m3 = t_m3;
        // readibility for counts
        size_t a_n = nold;
        size_t b_n = t_count_notna;
        size_t c_n = count_notna;

        // Running SD
        m2 += t_m2 + delta2 * (1.0 * a_n / count_notna * t_count_notna);

        // Running Skewness
        m3 += t_m3 + delta3 * a_n * b_n * (a_n - b_n)/(c_n * c_n);
        m3 += 3.0 * delta * (a_n * b_m2 - b_n * a_m2)/c_n;

        // Running Kurtosis
        m4 += t_m4 + delta4 * a_n * b_n * (a_n * a_n - a_n * b_n + b_n * b_n)/(pow(c_n,3));
        m4 += 6 * delta2 * (a_n * a_n * b_m2 + b_n * b_n * a_m2) / (c_n*c_n);
        m4 += 4 * delta * (a_n * b_m3 - b_n * a_m3) / c_n;

        // Running mean
        mean = static_cast<double>(sum) / count_notna;
      }
    });

  _countna = nrows - count_notna;
  if (count_notna == 0) {
    _min = _max = GETNA<T>();
    _mean = _sd = _skew = _kurt = GETNA<double>();
    _sum = 0;
  } else {
    _min = min;
    _max = max;
    _sum = sum;
    _mean = mean;
    _sd = count_notna > 1 ? std::sqrt(m2 / (count_notna - 1)) : 0;
    _skew = count_notna > 1 ? std::sqrt(count_notna) * m3/std::pow(m2,1.5) : 0;
    _kurt = count_notna > 1 ? (static_cast<double>(m4)*count_notna)/(m2*m2) : 0;
  }
  set_computed(Stat::Min);
  set_computed(Stat::Max);
  set_computed(Stat::Sum);
  set_computed(Stat::Mean);
  set_computed(Stat::StDev);
  set_computed(Stat::Skew);
  set_computed(Stat::Kurt);
  set_computed(Stat::NaCount);
}


template <typename T>
void NumericStats<T>::compute_sorted_stats(const Column* col) {
  Groupby grpby;
  RowIndex ri = col->_sort(&grpby);
  const int32_t* groups = grpby.offsets_r();
  size_t n_groups = grpby.ngroups();

  // Sorting gathers all NA elements at the top (in the first group). Thus if
  // we did not yet compute the NA count for the column, we can do so now by
  // checking whether the elements in the first group are NA or not.
  if (!is_computed(Stat::NaCount)) {
    T x0;
    bool r = col->get_element(ri[0], &x0);
    _countna = r? static_cast<size_t>(groups[1]) : 0;
    set_computed(Stat::NaCount);
  }

  bool has_nas = (_countna > 0);
  _nunique = n_groups - has_nas;
  set_computed(Stat::NUnique);

  size_t max_grpsize = 0;
  size_t best_igrp = 0;
  for (size_t i = has_nas; i < n_groups; ++i) {
    size_t grpsize = static_cast<size_t>(groups[i + 1] - groups[i]);
    if (grpsize > max_grpsize) {
      max_grpsize = grpsize;
      best_igrp = i;
    }
  }

  _nmodal = max_grpsize;
  size_t ig = static_cast<size_t>(groups[best_igrp]);
  bool isna = true;
  if (max_grpsize) {
    isna = col->get_element(ri[ig], &_mode);
  }
  if (isna) {
    _mode = GETNA<T>();  // TODO: remove
  }
  set_isna(Stat::Mode, isna);
  set_computed(Stat::NModal);
  set_computed(Stat::Mode);
}

template <typename T>
void NumericStats<T>::compute_minmax() {
  size_t nrows = column.nrows();
  size_t count_notna = 0;
  T min = infinity<T>();
  T max = -infinity<T>();
  std::mutex mutex;
  dt::parallel_region(
    [&] {
      size_t t_count_notna = 0;
      T t_min = infinity<T>();
      T t_max = -infinity<T>();

      dt::nested_for_static(nrows,
        [&](size_t i) {
          T x;
          bool isna = column.get_element(i, &x);
          if (isna) return;
          ++t_count_notna;
          if (x < t_min) t_min = x;  // Note: these ifs are not exclusive!
          if (x > t_max) t_max = x;
        });

      if (t_count_notna) {
        std::lock_guard<std::mutex> lock(mutex);
        count_notna += t_count_notna;
        if (t_min < min) min = t_min;
        if (t_max > max) max = t_max;
      }
    });

  _countna = nrows - count_notna;
  if (count_notna == 0) {
    set_isna(Stat::Min, true);
    set_isna(Stat::Max, true);
  } else {
    _min = min;
    _max = max;
    set_isna(Stat::Min, false);
    set_isna(Stat::Max, false);
  }
  set_computed(Stat::Min);
  set_computed(Stat::Max);
  set_computed(Stat::NaCount);
}


// template <typename T>
// double NumericStats<T>::sum(const Column* col) {
//   if (!is_computed(Stat::Sum)) compute_numerical_stats(col);
//   return _sum;
// }

// template <typename T>
// T NumericStats<T>::min(const Column* col) {
//   if (!is_computed(Stat::Min)) compute_numerical_stats(col);
//   return _min;
// }

// template <typename T>
// T NumericStats<T>::max(const Column* col) {
//   if (!is_computed(Stat::Max)) compute_numerical_stats(col);
//   return _max;
// }

// template <typename T>
// T NumericStats<T>::mode(const Column* col) {
//   if (!is_computed(Stat::Mode)) compute_sorted_stats(col);
//   return _mode;
// }

// template <typename T>
// double NumericStats<T>::mean(const Column* col) {
//   if (!is_computed(Stat::Mean)) compute_numerical_stats(col);
//   return _mean;
// }

// template <typename T>
// double NumericStats<T>::stdev(const Column* col) {
//   if (!is_computed(Stat::StDev)) compute_numerical_stats(col);
//   return _sd;
// }

// template <typename T>
// double NumericStats<T>::skew(const Column* col) {
//   if (!is_computed(Stat::Skew)) compute_numerical_stats(col);
//   return _skew;
// }

// template <typename T>
// double NumericStats<T>::kurt(const Column* col) {
//   if (!is_computed(Stat::Kurt)) compute_numerical_stats(col);
//   return _kurt;
// }

template<typename T>
void NumericStats<T>::compute_countna(const Column* col) {
  compute_numerical_stats(col);
}

template<typename T>
void NumericStats<T>::set_min(T value) {
  set_computed(Stat::Min, !ISNA<T>(value));
  _min = value;
}

template<typename T>
void NumericStats<T>::set_max(T value) {
  set_computed(Stat::Max, !ISNA<T>(value));
  _max = value;
}

//--------------

template<typename T>
bool NumericStats<T>::get_min(int64_t* out) {
  if (!std::is_integral<T>::value) return true;
  if (!is_computed(Stat::Min)) compute_minmax();
  *out = static_cast<int64_t>(_min);
  return is_na(Stat::Min);
}

template<typename T>
bool NumericStats<T>::get_max(int64_t* out) {
  if (!std::is_integral<T>::value) return true;
  if (!is_computed(Stat::Max)) compute_minmax();
  *out = static_cast<int64_t>(_max);
  return is_na(Stat::Max);
}

template<typename T>
bool NumericStats<T>::get_min(double* out) {
  if (!std::is_floating_point<T>::value) return true;
  if (!is_computed(Stat::Min)) compute_minmax();
  *out = static_cast<double>(_min);
  return is_na(Stat::Min);
}

template<typename T>
bool NumericStats<T>::get_max(double* out) {
  if (!std::is_floating_point<T>::value) return true;
  if (!is_computed(Stat::Max)) compute_minmax();
  *out = static_cast<double>(_max);
  return is_na(Stat::Max);
}



// template<typename T>
// void NumericStats<T>::verify_more(Stats* test, const Column* col) const
// {
//   auto ntest = static_cast<NumericStats<T>*>(test);
//   verify_stat(Stat::Min,   _min,   [&](){ return ntest->min(col); });
//   verify_stat(Stat::Max,   _max,   [&](){ return ntest->max(col); });
//   verify_stat(Stat::Sum,   _sum,   [&](){ return ntest->sum(col); });
//   verify_stat(Stat::Mean,  _mean,  [&](){ return ntest->mean(col); });
//   // verify_stat(Stat::StDev, _sd,    [&](){ return ntest->stdev(col); });
//   // verify_stat(Stat::Skew,  _skew,  [&](){ return ntest->skew(col); });
//   // verify_stat(Stat::Kurt,  _kurt,  [&](){ return ntest->kurt(col); });
// }

template class NumericStats<int64_t>;
template class NumericStats<double>;



//==============================================================================
// RealStats
//==============================================================================

// Adds a check for infinite/NaN mean and sd.
template <typename T>
void RealStats<T>::compute_numerical_stats(const Column* col) {
  NumericStats<double>::compute_numerical_stats(col);
  bool min_inf = (this->_min == -std::numeric_limits<double>::infinity());
  bool max_inf = (this->_max == +std::numeric_limits<double>::infinity());
  if (min_inf || max_inf) {
    this->_sd = GETNA<double>();
    this->_skew = GETNA<double>();
    this->_kurt = GETNA<double>();
    this->_mean = (min_inf && max_inf) ? GETNA<double>() :
                  (min_inf) ? static_cast<double>(this->_min)
                            : static_cast<double>(this->_max);
  }
}


template class RealStats<float>;
template class RealStats<double>;
template class IntegerStats<int32_t>;
template class IntegerStats<int64_t>;



//==============================================================================
// BooleanStats
//==============================================================================

/**
 * Compute standard deviation using the following formula derived from
 * `count0` and `count1`:
 *
 *            /              count0 * count1           \ 1/2
 *      sd = ( ---------------------------------------- )
 *            \ (count0 + count1 - 1)(count0 + count1) /
 */
void BooleanStats::compute_numerical_stats(const Column* col) {
  std::atomic<size_t> acountA { 0 };
  std::atomic<size_t> acount1 { 0 };
  size_t nrows = col->nrows;

  dt::parallel_region(
    [&] {
      size_t tcountA = 0;
      size_t tcount1 = 0;

      dt::nested_for_static(nrows,
        [&](size_t i) {
          int32_t x;
          bool r = col->get_element(i, &x);
          if (r) return;
          tcountA++;
          tcount1 += static_cast<uint32_t>(x);
        });

      acountA += tcountA;
      acount1 += tcount1;
    });
  size_t t_count = acountA.load();
  size_t count1 = acount1.load();
  size_t count0 = t_count - count1;
  double dcount0 = static_cast<double>(count0);
  double dcount1 = static_cast<double>(count1);
  _mean = t_count > 0 ? dcount1 / t_count : GETNA<double>();
  _sd = t_count > 1 ? std::sqrt(dcount0/t_count * dcount1/(t_count - 1))
                    : t_count == 1 ? 0 : GETNA<double>();
  _min = count0 ? 0 : count1 ? 1 : GETNA<int32_t>();
  _max = count1 ? 1 : count0 ? 0 : GETNA<int32_t>();
  _sum = static_cast<int64_t>(count1);
  _countna = nrows - t_count;
  _nunique = (!!count0) + (!!count1);
  _mode = _nunique ? (count1 >= count0) : GETNA<int32_t>();
  _nmodal = _mode == 1 ? count1 : _mode == 0 ? count0 : 0;
  set_computed(Stat::Max);
  set_computed(Stat::Mean);
  set_computed(Stat::Min);
  set_computed(Stat::Mode);
  set_computed(Stat::NaCount);
  set_computed(Stat::NModal);
  set_computed(Stat::NUnique);
  set_computed(Stat::StDev);
  set_computed(Stat::Sum);
}


void BooleanStats::compute_sorted_stats(const Column *col) {
  compute_numerical_stats(col);
}




//==============================================================================
// StringStats
//==============================================================================

void StringStats::compute_countna(const Column* col) {
  _countna = _count_nas<CString>(col);
  set_computed(Stat::NaCount);
}


void StringStats::compute_sorted_stats(const Column* col) {
  Groupby grpby;
  RowIndex ri = col->_sort(&grpby);
  const int32_t* groups = grpby.offsets_r();
  size_t n_groups = grpby.ngroups();

  if (!is_computed(Stat::NaCount)) {
    CString target;
    bool r = col->get_element(ri[0], &target);
    _countna = r? static_cast<size_t>(groups[1]) : 0;
    set_computed(Stat::NaCount);
  }

  bool has_nas = (_countna > 0);
  _nunique = n_groups - has_nas;
  set_computed(Stat::NUnique);

  size_t max_grpsize = 0;
  size_t best_igrp = 0;
  for (size_t i = has_nas; i < n_groups; ++i) {
    size_t grpsize = static_cast<size_t>(groups[i + 1] - groups[i]);
    if (grpsize > max_grpsize) {
      max_grpsize = grpsize;
      best_igrp = i;
    }
  }

  if (max_grpsize) {
    _nmodal = max_grpsize;
    size_t ig = static_cast<size_t>(groups[best_igrp]);
    col->get_element(ri[ig], &_mode);
  } else {
    _nmodal = 0;
    _mode = CString();
  }
  set_computed(Stat::NModal);
  set_computed(Stat::Mode);
}


struct StrHasher {
  size_t operator()(const CString& s) const {
    return hash_murmur2(s.ch, static_cast<size_t>(s.size));
  }
};

struct StrEqual {
  bool operator()(const CString& lhs, const CString& rhs) const {
    return (lhs.size == rhs.size) &&
           ((lhs.ch == rhs.ch) ||  // This ensures NAs are properly handled too
            (std::strncmp(lhs.ch, rhs.ch, static_cast<size_t>(lhs.size)) == 0));
  }
};


void StringStats::compute_nunique(const Column* col) {
  dt::shared_bmutex rwmutex;
  phmap::parallel_flat_hash_set<CString, StrHasher, StrEqual> values_seen;

  size_t batch_size = 8;
  size_t nbatches = (col->nrows + batch_size - 1) / batch_size;
  dt::parallel_for_dynamic(
    nbatches,
    [&](size_t i) {
      size_t j0 = i * batch_size;
      size_t j1 = std::min(j0 + batch_size, col->nrows);
      CString str;
      for (size_t j = j0; j < j1; ++j) {
        bool r = col->get_element(j, &str);
        if (r) continue;
        {
          dt::shared_lock<dt::shared_bmutex> lock(rwmutex, false);
          if (values_seen.contains(str)) continue;
        }
        {
          dt::shared_lock<dt::shared_bmutex> lock(rwmutex, true);
          values_seen.insert(str);
        }
      }
    });

  _nunique = values_seen.size();
  set_computed(Stat::NUnique);
}


CString StringStats::mode(const Column* col) {
  if (!is_computed(Stat::Mode)) compute_sorted_stats(col);
  return _mode;
}





//==============================================================================
// PyObjectStats
//==============================================================================

void PyObjectStats::compute_countna(const Column* col) {
  _countna = _count_nas<py::robj>(col);
  set_computed(Stat::NaCount);
}


void PyObjectStats::compute_sorted_stats(const Column*) {
  throw NotImplError();
}




//------------------------------------------------------------------------------
// OColumn's API
//------------------------------------------------------------------------------

static std::unique_ptr<Stats> _make_stats(const OColumn& col) {
  using pst = std::unique_ptr<Stats>;
  switch (col.stype()) {
    case SType::BOOL:    return pst(new BooleanStats(col));
    case SType::INT8:
    case SType::INT16:
    case SType::INT32:   return pst(new IntegerStats<int32_t>(col));
    case SType::INT64:   return pst(new IntegerStats<int64_t>(col));
    case SType::FLOAT32: return pst(new RealStats<float>(col));
    case SType::FLOAT64: return pst(new RealStats<double>(col));
    case SType::STR32:
    case SType::STR64:   return pst(new StringStats(col));
    case SType::OBJ:     return pst(new PyObjectStats(col));
    default:
      throw NotImplError()
        << "Cannot create Stats object for a column with type `"
        << col.stype() << '`';
  }
}

Stats* OColumn::get_stats() const {
  if (!pcol->stats) pcol->stats = _make_stats(*this);
  return pcol->stats.get();
}

Stats* OColumn::get_stats_if_exist() const {
  return pcol->stats.get();
}

void OColumn::reset_stats() {
  auto stats = get_stats_if_exist();
  if (stats) stats->reset();
}

bool OColumn::is_stat_computed(Stat stat) const {
  auto stats = get_stats_if_exist();
  return stats? stats->is_computed(stat) : false;
}


bool OColumn::get_stat(Stat stat, int64_t* out) const {
  return get_stats()->get_stat(stat, out);
}

bool OColumn::get_stat(Stat stat, double* out) const  {
  return get_stats()->get_stat(stat, out);
}

bool OColumn::get_stat(Stat stat, CString* out) const {
  return get_stats()->get_stat(stat, out);
}


template <typename T>
static py::oobj pywrap_stat(const OColumn& col, Stat stat) {
  T value;
  bool isna = col.get_stat(stat, &value);
  return isna? py::None() : py::oobj::wrap(value);
}

py::oobj OColumn::get_stat_as_pyobject(Stat stat) const {
  switch (stat) {
    case Stat::NaCount:
    case Stat::NUnique:
    case Stat::NModal: {
      return pywrap_stat<int64_t>(*this, stat);
    }
    case Stat::Sum:
    case Stat::Mean:
    case Stat::StDev:
    case Stat::Skew:
    case Stat::Kurt: {
      return pywrap_stat<double>(*this, stat);
    }
    case Stat::Min:
    case Stat::Max:
    case Stat::Mode: {
      switch (ltype()) {
        case LType::BOOL:
        case LType::INT:  return pywrap_stat<int64_t>(*this, stat);
        case LType::REAL: return pywrap_stat<double>(*this, stat);
        case LType::STRING: return pywrap_stat<CString>(*this, stat);
        default: return py::None();
      }
    }
    default:
      throw NotImplError() << "Cannot handle stat " << stat_name(stat);
  }
}




template <typename T>
static OColumn _make_column(SType stype, T value) {
  MemoryRange mbuf = MemoryRange::mem(sizeof(T));
  mbuf.set_element<T>(0, value);
  return OColumn::new_mbuf_column(stype, std::move(mbuf));
}

static OColumn _make_nacol(SType stype) {
  return OColumn::new_na_column(stype, 1);
}

static OColumn _make_column_str(CString value) {
  using T = uint32_t;
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
  return new_string_column(1, std::move(mbuf), std::move(strbuf));
}


template <typename T>
static OColumn colwrap_stat(const OColumn& col, Stat stat, SType stype) {
  T value;
  bool isna = col.get_stat(stat, &value);
  return isna? _make_nacol(stype)
             : _make_column<T>(stype, value);
}

static OColumn strcolwrap_stat(const OColumn& col, Stat stat) {
  CString value;
  bool isna = col.get_stat(stat, &value);
  return isna? _make_nacol(col.stype())
             : _make_column_str(value);
}


OColumn OColumn::get_stat_as_column(Stat stat) const {
  switch (stat) {
    case Stat::NaCount:
    case Stat::NUnique:
    case Stat::NModal: {
      return colwrap_stat<int64_t>(*this, stat, SType::INT64);
    }
    case Stat::Sum:
    case Stat::Mean:
    case Stat::StDev:
    case Stat::Skew:
    case Stat::Kurt: {
      return colwrap_stat<double>(*this, stat, SType::FLOAT64);
    }
    case Stat::Min:
    case Stat::Max:
    case Stat::Mode: {
      switch (ltype()) {
        case LType::BOOL:
        case LType::INT:  return colwrap_stat<int64_t>(*this, stat, SType::INT64);
        case LType::REAL: return colwrap_stat<double>(*this, stat, SType::FLOAT64);
        case LType::STRING: return strcolwrap_stat(*this, stat);
        default: return _make_nacol(stype());
      }
    }
    default:
      throw NotImplError();
  }
}
