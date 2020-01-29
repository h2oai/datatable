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
#include "column/column_impl.h"
#include "lib/parallel_hashmap/phmap.h"
#include "models/murmurhash.h"
#include "parallel/api.h"
#include "python/_all.h"
#include "python/string.h"
#include "utils/assert.h"
#include "utils/misc.h"
#include "column.h"
#include "datatablemodule.h"
#include "rowindex.h"
#include "stats.h"
#include "sort.h"


//------------------------------------------------------------------------------
// enum Stat helpers
//------------------------------------------------------------------------------

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
  throw RuntimeError() << "Unknown stat " << static_cast<int>(s);
}



//------------------------------------------------------------------------------
// main Stats class
//------------------------------------------------------------------------------

Stats::Stats(const dt::ColumnImpl* col) : column(col) {
  xassert(col);
}

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

void Stats::set_valid(Stat stat, bool isvalid) {
  _computed.set(static_cast<size_t>(stat), true);
  _valid.set(static_cast<size_t>(stat), isvalid);
}


template <typename T>
size_t NumericStats<T>::memory_footprint() const noexcept {
  return sizeof(NumericStats<T>);
}

size_t StringStats::memory_footprint() const noexcept {
  return sizeof(StringStats);
}

size_t PyObjectStats::memory_footprint() const noexcept {
  return sizeof(PyObjectStats);
}




//------------------------------------------------------------------------------
// Stats getters (generic)
//------------------------------------------------------------------------------

template <typename T>
static T _invalid(bool* isvalid) {
  if (isvalid) *isvalid = false;
  return T();
}


int64_t Stats::get_stat_int(Stat stat, bool* isvalid) {
  switch (stat) {
    case Stat::Min:  return min_int(isvalid);
    case Stat::Max:  return max_int(isvalid);
    case Stat::Mode: return mode_int(isvalid);
    default:         return _invalid<int64_t>(isvalid);
  }
}


size_t Stats::get_stat_uint(Stat stat, bool* isvalid) {
  switch (stat) {
    case Stat::NaCount: return nacount(isvalid);
    case Stat::NUnique: return nunique(isvalid);
    case Stat::NModal:  return nmodal(isvalid);
    default:            return _invalid<size_t>(isvalid);
  }
}


double Stats::get_stat_double(Stat stat, bool* isvalid) {
  switch (stat) {
    case Stat::Sum:   return sum(isvalid);
    case Stat::Mean:  return mean(isvalid);
    case Stat::StDev: return stdev(isvalid);
    case Stat::Skew:  return skew(isvalid);
    case Stat::Kurt:  return kurt(isvalid);
    case Stat::Min:   return min_double(isvalid);
    case Stat::Max:   return max_double(isvalid);
    case Stat::Mode:  return mode_double(isvalid);
    default:          return _invalid<double>(isvalid);
  }
}


CString Stats::get_stat_string(Stat stat, bool* isvalid) {
  switch (stat) {
    case Stat::Mode: return mode_string(isvalid);
    default:         return _invalid<CString>(isvalid);
  }
}


bool Stats::get_stat(Stat stat, int64_t* out) {
  bool ret;
  *out = get_stat_int(stat, &ret);
  return ret;
}

bool Stats::get_stat(Stat stat, size_t* out) {
  bool ret;
  *out = get_stat_uint(stat, &ret);
  return ret;
}

bool Stats::get_stat(Stat stat, double* out)   {
  bool ret;
  *out = get_stat_double(stat, &ret);
  return ret;
}

bool Stats::get_stat(Stat stat, CString* out)  {
  bool ret;
  *out = get_stat_string(stat, &ret);
  return ret;
}




//------------------------------------------------------------------------------
// Stats getters (specific)
//------------------------------------------------------------------------------

void Stats::_fill_validity_flag(Stat stat, bool* isvalid) {
  if (isvalid) *isvalid = is_valid(stat);
}

size_t Stats::nacount(bool* isvalid) {
  if (!is_computed(Stat::NaCount)) compute_nacount();
  _fill_validity_flag(Stat::NaCount, isvalid);
  return _countna;
}

size_t Stats::nunique(bool* isvalid) {
  if (!is_computed(Stat::NUnique)) compute_nunique();
  _fill_validity_flag(Stat::NUnique, isvalid);
  return _nunique;
}

size_t Stats::nmodal(bool* isvalid) {
  if (!is_computed(Stat::NModal)) compute_sorted_stats();
  _fill_validity_flag(Stat::NModal, isvalid);
  return _nmodal;
}


double Stats::sum  (bool* isvalid) { return _invalid<double>(isvalid); }
double Stats::mean (bool* isvalid) { return _invalid<double>(isvalid); }
double Stats::stdev(bool* isvalid) { return _invalid<double>(isvalid); }
double Stats::skew (bool* isvalid) { return _invalid<double>(isvalid); }
double Stats::kurt (bool* isvalid) { return _invalid<double>(isvalid); }

template <typename T>
double NumericStats<T>::sum(bool* isvalid) {
  if (!is_computed(Stat::Sum)) compute_moments12();
  _fill_validity_flag(Stat::Sum, isvalid);
  return _sum;
}

template <typename T>
double NumericStats<T>::mean(bool* isvalid) {
  if (!is_computed(Stat::Mean)) compute_moments12();
  _fill_validity_flag(Stat::Mean, isvalid);
  return _mean;
}

template <typename T>
double NumericStats<T>::stdev(bool* isvalid) {
  if (!is_computed(Stat::StDev)) compute_moments12();
  _fill_validity_flag(Stat::StDev, isvalid);
  return _stdev;
}

template <typename T>
double NumericStats<T>::skew(bool* isvalid) {
  if (!is_computed(Stat::Skew)) compute_moments34();
  _fill_validity_flag(Stat::Skew, isvalid);
  return _skew;
}

template <typename T>
double NumericStats<T>::kurt(bool* isvalid) {
  if (!is_computed(Stat::Kurt)) compute_moments34();
  _fill_validity_flag(Stat::Kurt, isvalid);
  return _kurt;
}

int64_t Stats::min_int    (bool* isvalid) { return _invalid<int64_t>(isvalid); }
int64_t Stats::max_int    (bool* isvalid) { return _invalid<int64_t>(isvalid); }
int64_t Stats::mode_int   (bool* isvalid) { return _invalid<int64_t>(isvalid); }
double  Stats::min_double (bool* isvalid) { return _invalid<double>(isvalid); }
double  Stats::max_double (bool* isvalid) { return _invalid<double>(isvalid); }
double  Stats::mode_double(bool* isvalid) { return _invalid<double>(isvalid); }
CString Stats::mode_string(bool* isvalid) { return _invalid<CString>(isvalid); }

template <typename T>
int64_t NumericStats<T>::min_int(bool* isvalid) {
  if (!std::is_integral<T>::value) return _invalid<int64_t>(isvalid);
  if (!is_computed(Stat::Min)) compute_minmax();
  _fill_validity_flag(Stat::Min, isvalid);
  xassert((std::is_same<decltype(_min), int64_t>::value));
  return static_cast<int64_t>(_min);
}

template <typename T>
int64_t NumericStats<T>::max_int(bool* isvalid) {
  if (!std::is_integral<T>::value) return _invalid<int64_t>(isvalid);
  if (!is_computed(Stat::Max)) compute_minmax();
  _fill_validity_flag(Stat::Max, isvalid);
  xassert((std::is_same<decltype(_max), int64_t>::value));
  return static_cast<int64_t>(_max);
}

template <typename T>
double NumericStats<T>::min_double(bool* isvalid) {
  if (!std::is_floating_point<T>::value) return _invalid<double>(isvalid);
  if (!is_computed(Stat::Min)) compute_minmax();
  _fill_validity_flag(Stat::Min, isvalid);
  xassert((std::is_same<decltype(_min), double>::value));
  return static_cast<double>(_min);
}

template <typename T>
double NumericStats<T>::max_double(bool* isvalid) {
  if (!std::is_floating_point<T>::value) return _invalid<double>(isvalid);
  if (!is_computed(Stat::Max)) compute_minmax();
  _fill_validity_flag(Stat::Max, isvalid);
  xassert((std::is_same<decltype(_max), double>::value));
  return static_cast<double>(_max);
}

template <typename T>
int64_t NumericStats<T>::mode_int(bool* isvalid) {
  if (!std::is_integral<T>::value) return _invalid<int64_t>(isvalid);
  if (!is_computed(Stat::Mode)) compute_sorted_stats();
  _fill_validity_flag(Stat::Mode, isvalid);
  xassert((std::is_same<decltype(_mode), int64_t>::value));
  return static_cast<int64_t>(_mode);
}

template <typename T>
double NumericStats<T>::mode_double(bool* isvalid) {
  if (!std::is_floating_point<T>::value) return _invalid<double>(isvalid);
  if (!is_computed(Stat::Mode)) compute_sorted_stats();
  _fill_validity_flag(Stat::Mode, isvalid);
  xassert((std::is_same<decltype(_mode), double>::value));
  return static_cast<double>(_mode);
}

CString StringStats::mode_string(bool* isvalid) {
  if (!is_computed(Stat::Mode)) compute_sorted_stats();
  _fill_validity_flag(Stat::Mode, isvalid);
  return _mode;
}




//------------------------------------------------------------------------------
// Stats setters (generic)
//------------------------------------------------------------------------------

void Stats::set_stat(Stat stat, int64_t value, bool isvalid) {
  switch (stat) {
    case Stat::Min:  return set_min(value, isvalid);
    case Stat::Max:  return set_max(value, isvalid);
    case Stat::Mode: return set_mode(value, isvalid);
    default: throw RuntimeError() << "Incorrect stat " << stat_name(stat);
  }
}

void Stats::set_stat(Stat stat, size_t value, bool isvalid) {
  switch (stat) {
    case Stat::NaCount: return set_nacount(value, isvalid);
    case Stat::NUnique: return set_nunique(value, isvalid);
    case Stat::NModal:  return set_nmodal(value, isvalid);
    default: throw RuntimeError() << "Incorrect stat " << stat_name(stat);
  }
}

void Stats::set_stat(Stat stat, double value, bool isvalid) {
  switch (stat) {
    case Stat::Sum:   return set_sum(value, isvalid);
    case Stat::Mean:  return set_mean(value, isvalid);
    case Stat::StDev: return set_stdev(value, isvalid);
    case Stat::Skew:  return set_skew(value, isvalid);
    case Stat::Kurt:  return set_kurt(value, isvalid);
    case Stat::Min:   return set_min(value, isvalid);
    case Stat::Max:   return set_max(value, isvalid);
    case Stat::Mode:  return set_mode(value, isvalid);
    default: throw RuntimeError() << "Incorrect stat " << stat_name(stat);
  }
}

void Stats::set_stat(Stat stat, const CString& value, bool isvalid) {
  switch (stat) {
    case Stat::Mode: return set_mode(value, isvalid);
    default: throw RuntimeError() << "Incorrect stat " << stat_name(stat);
  }
}




//------------------------------------------------------------------------------
// Stats setters (specific)
//------------------------------------------------------------------------------

void Stats::set_nacount(size_t value, bool isvalid) {
  xassert(isvalid);
  _countna = value;
  set_valid(Stat::NaCount, isvalid);
}

void Stats::set_nunique(size_t value, bool isvalid) {
  _nunique = value;
  set_valid(Stat::NUnique, isvalid);
}

void Stats::set_nmodal(size_t value, bool isvalid) {
  _nmodal = value;
  set_valid(Stat::NModal, isvalid);
}

void Stats::set_sum(double, bool)   { throw RuntimeError(); }
void Stats::set_mean(double, bool)  { throw RuntimeError(); }
void Stats::set_stdev(double, bool) { throw RuntimeError(); }
void Stats::set_skew(double, bool)  { throw RuntimeError(); }
void Stats::set_kurt(double, bool)  { throw RuntimeError(); }
void Stats::set_min(int64_t, bool)  { throw RuntimeError(); }
void Stats::set_min(double, bool)   { throw RuntimeError(); }
void Stats::set_max(int64_t, bool)  { throw RuntimeError(); }
void Stats::set_max(double, bool)   { throw RuntimeError(); }
void Stats::set_mode(int64_t, bool) { throw RuntimeError(); }
void Stats::set_mode(double, bool)  { throw RuntimeError(); }
void Stats::set_mode(CString, bool) { throw RuntimeError(); }


template <typename T>
void NumericStats<T>::set_sum(double value, bool isvalid) {
  _sum = value;
  set_valid(Stat::Sum, isvalid);
}

template <typename T>
void NumericStats<T>::set_mean(double value, bool isvalid) {
  _mean = value;
  set_valid(Stat::Mean, isvalid);
}

template <typename T>
void NumericStats<T>::set_stdev(double value, bool isvalid) {
  _stdev = value;
  set_valid(Stat::StDev, isvalid);
}

template <typename T>
void NumericStats<T>::set_skew(double value, bool isvalid) {
  _skew = value;
  set_valid(Stat::Skew, isvalid);
}

template <typename T>
void NumericStats<T>::set_kurt(double value, bool isvalid) {
  _kurt = value;
  set_valid(Stat::Kurt, isvalid);
}

template <typename T>
void NumericStats<T>::set_min(int64_t value, bool isvalid) {
  xassert((std::is_same<V, int64_t>::value));
  _min = static_cast<V>(value);
  set_valid(Stat::Min, isvalid);
}

template <typename T>
void NumericStats<T>::set_min(double value, bool isvalid) {
  xassert((std::is_same<V, double>::value));
  _min = static_cast<V>(value);
  set_valid(Stat::Min, isvalid);
}

template <typename T>
void NumericStats<T>::set_max(int64_t value, bool isvalid) {
  xassert((std::is_same<V, int64_t>::value));
  _max = static_cast<V>(value);
  set_valid(Stat::Max, isvalid);
}

template <typename T>
void NumericStats<T>::set_max(double value, bool isvalid) {
  xassert((std::is_same<V, double>::value));
  _max = static_cast<V>(value);
  set_valid(Stat::Max, isvalid);
}

template <typename T>
void NumericStats<T>::set_mode(int64_t value, bool isvalid) {
  xassert((std::is_same<V, int64_t>::value));
  _mode = static_cast<V>(value);
  set_valid(Stat::Mode, isvalid);
}

template <typename T>
void NumericStats<T>::set_mode(double value, bool isvalid) {
  xassert((std::is_same<V, double>::value));
  _mode = static_cast<V>(value);
  set_valid(Stat::Mode, isvalid);
}

void StringStats::set_mode(CString value, bool isvalid) {
  _mode = value;
  set_valid(Stat::Mode, isvalid);
}




//------------------------------------------------------------------------------
// Stats computation: NaCount
//------------------------------------------------------------------------------

template <typename T>
static size_t _compute_nacount(const dt::ColumnImpl* col) {
  assert_compatible_type<T>(col->stype());
  std::atomic<size_t> total_countna { 0 };
  dt::parallel_region(
    dt::NThreads(col->allow_parallel_access()),
    [&] {
      T target;
      size_t thread_countna = 0;
      dt::nested_for_static(col->nrows(),
        [&](size_t i) {
          bool isvalid = col->get_element(i, &target);
          thread_countna += !isvalid;
        });
      total_countna += thread_countna;
    });
  return total_countna.load();
}

void Stats::compute_nacount() { throw NotImplError(); }

template <typename T>
void NumericStats<T>::compute_nacount() {
  set_nacount(_compute_nacount<T>(column), true);
}

void BooleanStats::compute_nacount() {
  compute_all_stats();
}

void StringStats::compute_nacount() {
  set_nacount(_compute_nacount<CString>(column), true);
}

void PyObjectStats::compute_nacount() {
  set_nacount(_compute_nacount<py::robj>(column), true);
}




//------------------------------------------------------------------------------
// Stats computation: Min + Max
//------------------------------------------------------------------------------

template<typename T>
constexpr T infinity() {
  return std::numeric_limits<T>::has_infinity
         ? std::numeric_limits<T>::infinity()
         : std::numeric_limits<T>::max();
}

void Stats::compute_minmax() {
  set_valid(Stat::Min, false);
  set_valid(Stat::Max, false);
}

void BooleanStats::compute_minmax() {
  compute_all_stats();
}

template <typename T>
void NumericStats<T>::compute_minmax() {
  assert_compatible_type<T>(column->stype());
  size_t nrows = column->nrows();
  size_t count_valid = 0;
  T min = infinity<T>();
  T max = -infinity<T>();
  std::mutex mutex;
  dt::parallel_region(
    dt::NThreads(column->allow_parallel_access()),
    [&] {
      size_t t_count_notna = 0;
      T t_min = infinity<T>();
      T t_max = -infinity<T>();

      dt::nested_for_static(nrows,
        [&](size_t i) {
          T x;
          bool isvalid = column->get_element(i, &x);
          if (!isvalid) return;
          t_count_notna++;
          if (x < t_min) t_min = x;  // Note: these ifs are not exclusive!
          if (x > t_max) t_max = x;
        });

      if (t_count_notna) {
        std::lock_guard<std::mutex> lock(mutex);
        count_valid += t_count_notna;
        if (t_min < min) min = t_min;
        if (t_max > max) max = t_max;
      }
    });
  set_nacount(nrows - count_valid, true);
  set_min(static_cast<V>(min), (count_valid > 0));
  set_max(static_cast<V>(max), (count_valid > 0));
}





//------------------------------------------------------------------------------
// Stats computation: NUnique
//------------------------------------------------------------------------------

void Stats::compute_nunique() {
  set_valid(Stat::NUnique, false);
}

template <typename T>
void NumericStats<T>::compute_nunique() {
  compute_sorted_stats();
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

void BooleanStats::compute_nunique() {
  compute_all_stats();
}

void StringStats::compute_nunique() {
  dt::shared_bmutex rwmutex;
  phmap::parallel_flat_hash_set<CString, StrHasher, StrEqual> values_seen;

  size_t batch_size = 8;
  size_t nbatches = (column->nrows() + batch_size - 1) / batch_size;
  dt::parallel_for_dynamic(
    nbatches,
    [&](size_t i) {
      size_t j0 = i * batch_size;
      size_t j1 = std::min(j0 + batch_size, column->nrows());
      CString str;
      for (size_t j = j0; j < j1; ++j) {
        bool isvalid = column->get_element(j, &str);
        if (!isvalid) continue;
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
  set_nunique(values_seen.size());
}




//------------------------------------------------------------------------------
// Stats computation: Sum + Mean + StDev
//------------------------------------------------------------------------------

void Stats::compute_moments12() {
  set_valid(Stat::Sum, false);
  set_valid(Stat::Mean, false);
  set_valid(Stat::StDev, false);
}


/**
 * Standard deviation and mean computations are done using Welford's method.
 * In particular, if m1[n-1] is the mean of n-1 observations x[1]...x[n-1], then
 *
 *     m1[n] = 1/n * (x[1] + x[2] + ... + x[n-1] + x[n])
 *           = 1/n * ((n-1)*m1[n-1] + x[n])
 *           = m1[n-1] + (x[n] - m1[n-1])/n
 *           = m1[n-1] + delta1/n
 *
 * Similarly, for the second central moment:
 *
 *     M2[n] = (x[1] - m1[n])^2 + ... + (x[n] - m1[n])^2
 *           = x[1]^2 + ... + x[n]^2 - 2*m1[n]*(x[1]+...+x[n]) + n*m1[n]^2
 *           = (x[1]^2 + ... + x[n]^2) - n*m1[n]^2
 *           = M2[n-1] + (n-1)*m1[n-1]^2 + x[n]^2 - n*m1[n]^2
 *           = M2[n-1] + x[n]^2 - m1[n-1]^2 - n*(m1[n]^2 - m1[n-1]^2)
 *           = M2[n-1] + delta1*(x[n] + m1[n-1]) - delta1*(m1[n] + m1[n-1])
 *           = M2[n-1] + delta1*(x[n] - m1[n])
 *           = M2[n-1] + delta1*delta2
 *
 * where `delta1 = x[n] - m1[n-1]` and `delta2 = x[n] - m1[n]`.
 *
 * References:
 * [Pebay2008] P. Pébay. Formulas for Robust, One-Pass Parallel Computation of
 *             Covariances and Arbitrary-Order Statistical Moments. Sandia
 *             Report, 2008.
 *             https://prod-ng.sandia.gov/techlib-noauth/access-control.cgi/2008/086212.pdf
 */
template <typename T>
void NumericStats<T>::compute_moments12() {
  size_t nrows = column->nrows();
  size_t count = 0;
  bool has_pos_inf = false;
  bool has_neg_inf = false;
  double sum = 0;
  double mean = 0;
  double M2 = 0;

  std::mutex mutex;
  dt::parallel_region(
    dt::NThreads(column->allow_parallel_access()),
    [&] {
      size_t t_count = 0;
      bool t_has_pos_inf = false;
      bool t_has_neg_inf = false;
      double t_sum = 0.0;
      double t_mean = 0.0;
      double t_M2 = 0.0;

      dt::nested_for_static(nrows,
        [&](size_t i) {
          T value;
          bool isvalid = column->get_element(i, &value);
          if (!isvalid) return;
          double x = static_cast<double>(value);
          t_count++;
          t_sum += x;
          double delta1 = x - t_mean;
          t_mean += delta1 / t_count;
          double delta2 = x - t_mean;
          t_M2 += delta1 * delta2;
          if (std::is_floating_point<T>::value) {
            t_has_pos_inf |= (value == std::numeric_limits<T>::infinity());
            t_has_neg_inf |= (value == -std::numeric_limits<T>::infinity());
          }
        });

      if (t_count) {
        std::lock_guard<std::mutex> lock(mutex);
        size_t n1 = count;
        size_t n2 = t_count;
        size_t n = n1 + n2;
        double delta21 = t_mean - mean;
        count = n;
        sum += t_sum;
        mean += delta21 * n2 / n;
        M2 += t_M2 + delta21 * delta21 * n1 / n * n2;
        has_pos_inf |= t_has_pos_inf;
        has_neg_inf |= t_has_neg_inf;
      }
    });

  size_t n = count;
  bool sum_valid = true;
  bool mean_valid = (n > 0);
  bool stdev_valid = (n > 0);
  double s = n > 1 ? std::sqrt(M2 / (n - 1)) : 0.0;

  // Adjustment for the case when some of the `x[i]`s where infinite.
  if (std::is_floating_point<T>::value && (has_pos_inf || has_neg_inf)) {
    constexpr double nan = std::numeric_limits<double>::quiet_NaN();
    constexpr double inf = std::numeric_limits<double>::infinity();
    s = nan;
    stdev_valid = false;
    if (has_pos_inf && has_neg_inf) {
      sum = mean = s = nan;
      sum_valid = mean_valid = false;
    }
    else if (has_pos_inf) {
      sum = mean = inf;
    }
    else if (has_neg_inf) {
      sum = mean = -inf;
    }
    if (has_pos_inf) set_max(inf, true);
    if (has_neg_inf) set_min(-inf, true);
  }

  set_nacount(nrows - n, true);
  set_sum(sum, sum_valid);
  set_mean(mean, mean_valid);
  set_stdev(s, stdev_valid);
}

void BooleanStats::compute_moments12() {
  compute_all_stats();
}




//------------------------------------------------------------------------------
// Stats computation: Skew + Kurt
//------------------------------------------------------------------------------

void Stats::compute_moments34() {
  set_valid(Stat::Skew, false);
  set_valid(Stat::Kurt, false);
}


/**
 * The [Pebay 2008] paper linked above gives formulas for parallel computation
 * of third and fourth central moments too:
 *
 * delta = x[n] - m1[n-1]
 * M3[n] = M3[n-1] + (n-1)*(n-2)*delta^3/n^2 - 3*M2[n-1]*delta/n
 * M4[n] = M4[n-1] + (n-1)*(n^2 - 3n + 3)*delta^4/n^3 + 6*M2[n-1]*delta^2/n^2
 *         -4*M3[n-1]*delta/n
 */
template <typename T>
void NumericStats<T>::compute_moments34() {
  size_t nrows = column->nrows();
  size_t count = 0;
  double sum = 0.0;   // x[1] + ... + x[n]
  double mean = 0.0;  // sum / n
  double M2 = 0.0;    // (x[1] - mean)^2 + ... (x[n] - mean)^2
  double M3 = 0.0;    // (x[1] - mean)^3 + ... (x[n] - mean)^3
  double M4 = 0.0;    // (x[1] - mean)^4 + ... (x[n] - mean)^4

  std::mutex mutex;
  dt::parallel_region(
    dt::NThreads(column->allow_parallel_access()),
    [&] {
      size_t t_count = 0;
      double t_sum = 0.0;
      double t_mean = 0.0;
      double t_M2 = 0.0;
      double t_M3 = 0.0;
      double t_M4 = 0.0;

      dt::nested_for_static(nrows,
        [&](size_t i) {
          T value;
          bool isvalid = column->get_element(i, &value);
          if (!isvalid) return;
          double x = static_cast<double>(value);
          ++t_count;
          size_t n = t_count; // readability
          double delta = x - t_mean;                // δ
          double gamma = delta / n;                 // δ/n
          double beta = gamma * gamma;              // δ²/n²
          double alpha = delta * gamma * (n - 1);   // δ²(n-1)/n
          t_sum += x;
          t_mean += gamma;
          t_M4 += (alpha * (n*n - 3*n + 3) + 6*t_M2) * beta - 4*gamma * t_M3;
          t_M3 += (alpha * (n - 2) - 3*t_M2) * gamma;
          t_M2 += alpha;
        });

      if (t_count) {
        std::lock_guard<std::mutex> lock(mutex);
        size_t n1 = count;
        size_t n2 = t_count;
        size_t n = n1 + n2;
        double delta21 = t_mean - mean;
        double gamma21 = delta21 / n;
        double beta21  = gamma21 * gamma21;
        double alpha21 = delta21 * delta21 * n1 * n2 / n;
        double M2_1 = M2;
        double M2_2 = t_M2;
        double M3_1 = M3;
        double M3_2 = t_M3;
        double M4_1 = M4;
        double M4_2 = t_M4;

        count = n;
        sum += t_sum;
        mean += gamma21 * n2;
        M2 = M2_1 + M2_2 + alpha21;
        M3 = M3_1 + M3_2 + alpha21 * (n1 - n2)/n
             + 3.0 * (n1 * M2_2 - n2 * M2_1) * gamma21;
        M4 = M4_1 + M4_2 + alpha21 * beta21 * (n1*n1 - n1*n2 + n2*n2)
             + 6 * beta21 * (n1*n1 * M2_2 + n2*n2 * M2_1)
             + 4 * gamma21 * (n1 * M3_2 - n2 * M3_1);
      }
    });

  size_t n = count;
  double s = (n > 1) ? std::sqrt(M2 / (n - 1)) : 0.0;
  double G = (n > 2) ? M3 / std::pow(s, 3) * n /(n-1) /(n-2) : 0.0;
  double K = (n > 3) ? (M4 / std::pow(s, 4) * n*(n+1)
                        - 3.0*(n-1)*(n-1)*(n-1)) /(n-1) /(n-2) /(n-3) : 0.0;

  set_nacount(nrows - n, true);
  set_sum(sum, true);
  set_mean(mean, (n > 0));
  set_stdev(s, (n > 0));
  set_skew(G, (n > 0));
  set_kurt(K, (n > 0));
}


void BooleanStats::compute_moments34() {
  compute_all_stats();
}



//------------------------------------------------------------------------------
// Stats computation: Mode, NModal
//------------------------------------------------------------------------------

void Stats::compute_sorted_stats() {
  set_valid(Stat::Mode, false);
  set_valid(Stat::NModal, false);
}


template <typename T>
void NumericStats<T>::compute_sorted_stats() {
  auto r = group({Column(column->clone())}, {SortFlag::NONE});
  RowIndex ri   = std::move(r.first);
  Groupby grpby = std::move(r.second);
  if (column->nrows() == 0) {
    grpby = Groupby::single_group(0);
  }

  const int32_t* groups = grpby.offsets_r();
  size_t n_groups = grpby.size();
  xassert(n_groups >= 1);

  // Sorting gathers all NA elements at the top (in the first group). Thus if
  // we did not yet compute the NA count for the column, we can do so now by
  // checking whether the elements in the first group are NA or not.
  if (!is_computed(Stat::NaCount)) {
    T x0;
    size_t ri0;
    bool isvalid = (ri.size() == 0) ||
                   (ri.get_element(0, &ri0) &&
                    column->get_element(ri0, &x0));
    set_nacount(isvalid? 0 : static_cast<size_t>(groups[1]));
  }

  bool has_nas = (_countna > 0);
  bool grp_empty = (n_groups == 1) && (groups[1] == 0);
  set_nunique(n_groups - has_nas - grp_empty, true);

  size_t max_group_size = 0;
  size_t largest_group_index = 0;
  for (size_t i = has_nas; i < n_groups; ++i) {
    size_t grpsize = static_cast<size_t>(groups[i + 1] - groups[i]);
    if (grpsize > max_group_size) {
      max_group_size = grpsize;
      largest_group_index = i;
    }
  }

  size_t ig = static_cast<size_t>(groups[largest_group_index]);
  T mode_value {};
  size_t ri_ig;
  bool mode_valid = max_group_size &&
                    ri.get_element(ig, &ri_ig) &&
                    column->get_element(ri_ig, &mode_value);
  set_mode(static_cast<V>(mode_value), mode_valid);
  set_nmodal(max_group_size, true);
}


void StringStats::compute_sorted_stats() {
  auto r = group({Column(column->clone())}, {SortFlag::NONE});
  RowIndex ri   = std::move(r.first);
  Groupby grpby = std::move(r.second);
  if (column->nrows() == 0) {
    grpby = Groupby::single_group(0);
  }

  const int32_t* groups = grpby.offsets_r();
  size_t n_groups = grpby.size();
  xassert(n_groups >= 1);

  // Sorting gathers all NA elements at the top (in the first group). Thus if
  // we did not yet compute the NA count for the column, we can do so now by
  // checking whether the elements in the first group are NA or not.
  if (!is_computed(Stat::NaCount)) {
    CString x0;
    size_t ri0;
    bool isvalid = (ri.size() == 0) ||
                   (ri.get_element(0, &ri0) &&
                    column->get_element(ri0, &x0));
    set_nacount(isvalid? 0 : static_cast<size_t>(groups[1]));
  }

  bool has_nas = (_countna > 0);
  bool grp_empty = (n_groups == 1) && (groups[1] == 0);
  set_nunique(n_groups - has_nas - grp_empty, true);

  size_t max_group_size = 0;
  size_t largest_group_index = 0;
  for (size_t i = has_nas; i < n_groups; ++i) {
    size_t grpsize = static_cast<size_t>(groups[i + 1] - groups[i]);
    if (grpsize > max_group_size) {
      max_group_size = grpsize;
      largest_group_index = i;
    }
  }

  size_t ig = static_cast<size_t>(groups[largest_group_index]);
  CString mode_value;
  size_t ri_ig;
  bool mode_valid = max_group_size &&
                    ri.get_element(ig, &ri_ig) &&
                    column->get_element(ri_ig, &mode_value);
  set_mode(mode_value, mode_valid);
  set_nmodal(max_group_size, true);
}


void BooleanStats::compute_sorted_stats() {
  compute_all_stats();
}




//------------------------------------------------------------------------------
// BooleanStats: compute all
//------------------------------------------------------------------------------

/**
 * For boolean column, all statistics can be computed from just two
 * quantities: count of 0s `n0`, and count of 1s `n1`. Then:
 *
 *     n = n0 + n1
 *     µ = n1 / n
 *     s^2 = n0*n1 / (n*(n-1))
 *     G = (n0 - n1) / ((n-2) * s)
 *     K = (n-1)/((n-2)(n-3)) * ((n+1)*(n0^2 + n0*n1 + n1^2)/(n0*n1) - 3(n-1))
 */
void BooleanStats::compute_all_stats() {
  size_t nrows = column->nrows();
  std::atomic<size_t> count_all { 0 };
  std::atomic<size_t> count_1 { 0 };

  dt::parallel_region(
    dt::NThreads(column->allow_parallel_access()),
    [&] {
      size_t t_count_all = 0;
      size_t t_count_1 = 0;

      dt::nested_for_static(nrows,
        [&](size_t i) {
          int8_t x;
          bool isvalid = column->get_element(i, &x);
          if (!isvalid) return;
          t_count_all++;
          t_count_1 += static_cast<size_t>(x);
        });

      count_all += t_count_all;
      count_1 += t_count_1;
    });
  size_t n = count_all.load();
  size_t n1 = count_1.load();
  size_t n0 = n - n1;

  bool valid = (n > 0);
  double mu = valid ? 1.0 * n1 / n : 0.0;
  double s = (n > 1) ? std::sqrt(1.0 * n0 * n1 / n / (n-1)) : 0.0;
  double G = (n > 2) ? 1.0 * static_cast<int64_t>(n0 - n1) / (n-2) / s : 0.0;
  double K = (n > 3) ? (1.0 * static_cast<int64_t>(n0*n0 - n0*n1 + n1*n1)
                            * (n+1)/n0/n1 - 3.0*(n-1))
                       * (n-1) / (n-2) / (n-3) : 0.0;

  set_nacount(nrows - n, true);
  set_nunique((n0 > 0) + (n1 > 0), true);
  set_nmodal(std::max(n0, n1), true);
  set_sum(static_cast<double>(n1), true);
  set_mean(mu, valid);
  set_stdev(s, valid);
  set_skew(G, valid);
  set_kurt(K, valid);
  set_min(int64_t(n0? 0 : 1), valid);
  set_max(int64_t(n1? 1 : 0), valid);
  set_mode(int64_t(n0>=n1? 0 : 1), valid);
}




//------------------------------------------------------------------------------
// Column's API
//------------------------------------------------------------------------------

static std::unique_ptr<Stats> _make_stats(const dt::ColumnImpl* col) {
  using StatsPtr = std::unique_ptr<Stats>;
  switch (col->stype()) {
    case SType::BOOL:    return StatsPtr(new BooleanStats(col));
    case SType::INT8:    return StatsPtr(new IntegerStats<int8_t>(col));
    case SType::INT16:   return StatsPtr(new IntegerStats<int16_t>(col));
    case SType::INT32:   return StatsPtr(new IntegerStats<int32_t>(col));
    case SType::INT64:   return StatsPtr(new IntegerStats<int64_t>(col));
    case SType::FLOAT32: return StatsPtr(new RealStats<float>(col));
    case SType::FLOAT64: return StatsPtr(new RealStats<double>(col));
    case SType::STR32:
    case SType::STR64:   return StatsPtr(new StringStats(col));
    case SType::OBJ:     return StatsPtr(new PyObjectStats(col));
    default:
      throw NotImplError()
        << "Cannot create Stats object for a column with type `"
        << col->stype() << '`';
  }
}

Stats* Column::stats() const {
  if (!impl_->stats_) impl_->stats_ = _make_stats(impl_);
  return impl_->stats_.get();
}

Stats* Column::get_stats_if_exist() const {
  return impl_->stats_.get();
}


void Column::reset_stats() {
  auto stats = get_stats_if_exist();
  if (stats) stats->reset();
}


std::unique_ptr<Stats> Column::clone_stats() const {
  return (impl_->stats_)? impl_->stats_->clone()
                        : std::unique_ptr<Stats>();
}


void Column::replace_stats(std::unique_ptr<Stats>&& new_stats) {
  new_stats->column = impl_;
  impl_->stats_ = std::move(new_stats);
}


bool Column::is_stat_computed(Stat stat) const {
  auto stats = get_stats_if_exist();
  return stats? stats->is_computed(stat) : false;
}




//------------------------------------------------------------------------------
// Stats cloning
//------------------------------------------------------------------------------

template <typename S>
std::unique_ptr<Stats> Stats::_clone(const S* inp) const {
  S* out = new S(inp->column);
  std::memcpy(static_cast<void*>(out),
              static_cast<const void*>(inp),
              sizeof(S));
  xassert(out->_valid == inp->_valid);
  xassert(out->_computed == inp->_computed);
  return std::unique_ptr<Stats>(out);
}


template <typename T>
std::unique_ptr<Stats> RealStats<T>::clone()    const { return this->_clone(this); }
template <typename T>
std::unique_ptr<Stats> IntegerStats<T>::clone() const { return this->_clone(this); }
std::unique_ptr<Stats> BooleanStats::clone()    const { return this->_clone(this); }
std::unique_ptr<Stats> StringStats::clone()     const { return this->_clone(this); }
std::unique_ptr<Stats> PyObjectStats::clone()   const { return this->_clone(this); }




//------------------------------------------------------------------------------
// Stats integrity checks
//------------------------------------------------------------------------------

template <typename T>
inline bool _equal(T a, T b) { return a == b; }

template<>
inline bool _equal(float a, float b) {
  // Equality check is needed to ensure that inf==inf
  return (a == b) || (std::abs(a - b) < 1e-7f);
}

template<>
inline bool _equal(double a, double b) {
  return (a == b) || (std::abs(a - b) < 1e-12);
}

template <typename T>
static void check_stat(Stat stat, Stats* curr_stats, Stats* new_stats) {
  if (!curr_stats->is_computed(stat)) return;
  T value1, value2;
  bool isvalid1 = curr_stats->get_stat(stat, &value1);
  bool isvalid2 = new_stats->get_stat(stat, &value2);
  if (isvalid1 != isvalid2) {
    throw AssertionError() << "Stat " << stat_name(stat) << " is recorded as "
      "valid=" << isvalid1 << " in the Stats object, but was valid=" << isvalid2
      << " upon re-checking";
  }
  if (isvalid1 && !_equal(value1, value2)) {
    throw AssertionError() << "Stat " << stat_name(stat) << "'s value is "
      << value1 << ", but it was " << value2 << " upon recalculation";
  }
}

void Stats::verify_integrity(const dt::ColumnImpl* col) {
  XAssert(column == col);
  switch (col->stype()) {
    case SType::BOOL:    XAssert(dynamic_cast<BooleanStats*>(this)); break;
    case SType::INT8:    XAssert(dynamic_cast<IntegerStats<int8_t>*>(this)); break;
    case SType::INT16:   XAssert(dynamic_cast<IntegerStats<int16_t>*>(this)); break;
    case SType::INT32:   XAssert(dynamic_cast<IntegerStats<int32_t>*>(this)); break;
    case SType::INT64:   XAssert(dynamic_cast<IntegerStats<int64_t>*>(this)); break;
    case SType::FLOAT32: XAssert(dynamic_cast<RealStats<float>*>(this)); break;
    case SType::FLOAT64: XAssert(dynamic_cast<RealStats<double>*>(this)); break;
    case SType::STR32:
    case SType::STR64:   XAssert(dynamic_cast<StringStats*>(this)); break;
    case SType::OBJ:     XAssert(dynamic_cast<PyObjectStats*>(this)); break;
    default: throw AssertionError() << "Unknown column type " << col->stype();
  }
  auto new_stats = _make_stats(column);
  check_stat<size_t>(Stat::NaCount, this, new_stats.get());
  check_stat<size_t>(Stat::NUnique, this, new_stats.get());
  check_stat<size_t>(Stat::NModal, this, new_stats.get());
  check_stat<double>(Stat::Sum, this, new_stats.get());
  check_stat<double>(Stat::Mean, this, new_stats.get());
  check_stat<double>(Stat::StDev, this, new_stats.get());
  check_stat<double>(Stat::Skew, this, new_stats.get());
  check_stat<double>(Stat::Kurt, this, new_stats.get());
  check_stat<double>(Stat::Min, this, new_stats.get());
  check_stat<double>(Stat::Max, this, new_stats.get());
  check_stat<double>(Stat::Mode, this, new_stats.get());
  check_stat<int64_t>(Stat::Min, this, new_stats.get());
  check_stat<int64_t>(Stat::Max, this, new_stats.get());
  check_stat<int64_t>(Stat::Mode, this, new_stats.get());
  check_stat<CString>(Stat::Mode, this, new_stats.get());
}




//------------------------------------------------------------------------------
// Stats "pyobject" getter
//------------------------------------------------------------------------------

template <typename S>
py::oobj Stats::pywrap_stat(Stat stat) {
  S value;
  bool isvalid = get_stat(stat, &value);
  return isvalid? py::oobj::wrap(value) : py::None();
}


py::oobj Stats::get_stat_as_pyobject(Stat stat) {
  switch (stat) {
    case Stat::NaCount:
    case Stat::NUnique:
    case Stat::NModal: {
      return pywrap_stat<size_t>(stat);
    }
    case Stat::Sum:
    case Stat::Mean:
    case Stat::StDev:
    case Stat::Skew:
    case Stat::Kurt: {
      return pywrap_stat<double>(stat);
    }
    case Stat::Min:
    case Stat::Max:
    case Stat::Mode: {
      switch (info(column->stype()).ltype()) {
        case LType::BOOL:
        case LType::INT:  return pywrap_stat<int64_t>(stat);
        case LType::REAL: return pywrap_stat<double>(stat);
        case LType::STRING: return pywrap_stat<CString>(stat);
        default: return py::None();
      }
    }
    default:
      throw NotImplError() << "Cannot handle stat " << stat_name(stat);
  }
}




//------------------------------------------------------------------------------
// Stats "Column" getter
//------------------------------------------------------------------------------

template <typename T>
static Column _make_column(SType stype, T value) {
  Buffer mbuf = Buffer::mem(sizeof(T));
  mbuf.set_element<T>(0, value);
  Column res = Column::new_mbuf_column(1, stype, std::move(mbuf));
  xassert(res.nrows() == 1);
  return res;
}

static Column _make_nacol(SType stype) {
  return Column::new_na_column(1, stype);
}

static Column _make_column_str(CString value) {
  using T = uint32_t;
  Buffer mbuf = Buffer::mem(sizeof(T) * 2);
  Buffer strbuf;
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
  return Column::new_string_column(1, std::move(mbuf), std::move(strbuf));
}


template <typename S, typename R>
Column Stats::colwrap_stat(Stat stat, SType stype) {
  S value;
  bool isvalid = get_stat(stat, &value);
  return isvalid? _make_column<R>(stype, static_cast<R>(value))
                : _make_nacol(stype);
}

Column Stats::strcolwrap_stat(Stat stat) {
  CString value;
  bool isvalid = get_stat(stat, &value);
  return isvalid? _make_column_str(value)
                : _make_nacol(column->stype());
}


Column Stats::get_stat_as_column(Stat stat) {
  switch (stat) {
    case Stat::NaCount:
    case Stat::NUnique:
    case Stat::NModal: {
      return colwrap_stat<size_t, int64_t>(stat, SType::INT64);
    }
    case Stat::Sum:
    case Stat::Mean:
    case Stat::StDev:
    case Stat::Skew:
    case Stat::Kurt: {
      return colwrap_stat<double, double>(stat, SType::FLOAT64);
    }
    case Stat::Min:
    case Stat::Max:
    case Stat::Mode: {
      switch (column->stype()) {
        case SType::BOOL:    return colwrap_stat<int64_t, int8_t>(stat, SType::BOOL);
        case SType::INT8:    return colwrap_stat<int64_t, int8_t>(stat, SType::INT8);
        case SType::INT16:   return colwrap_stat<int64_t, int16_t>(stat, SType::INT16);
        case SType::INT32:   return colwrap_stat<int64_t, int32_t>(stat, SType::INT32);
        case SType::INT64:   return colwrap_stat<int64_t, int64_t>(stat, SType::INT64);
        case SType::FLOAT32: return colwrap_stat<double, float>(stat, SType::FLOAT32);
        case SType::FLOAT64: return colwrap_stat<double, double>(stat, SType::FLOAT64);
        case SType::STR32:
        case SType::STR64:   return strcolwrap_stat(stat);
        default:             return _make_nacol(column->stype());
      }
    }
    default:
      throw NotImplError();
  }
}




//------------------------------------------------------------------------------
// Instantiate templates
//------------------------------------------------------------------------------

template class RealStats<float>;
template class RealStats<double>;
template class IntegerStats<int8_t>;
template class IntegerStats<int16_t>;
template class IntegerStats<int32_t>;
template class IntegerStats<int64_t>;
