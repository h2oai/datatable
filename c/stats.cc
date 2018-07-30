//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "stats.h"
#include <cmath>        // std::isinf, std::sqrt
#include <limits>       // std::numeric_limits
#include <type_traits>  // std::is_floating_point
#include "column.h"
#include "rowindex.h"
#include "utils.h"
#include "utils/omp.h"


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
}



//==============================================================================
// Base Stats
//==============================================================================

void Stats::reset() {
  _computed.reset();
}

bool Stats::is_computed(Stat s) const {
  return _computed.test(static_cast<size_t>(s));
}

void Stats::set_computed(Stat s) {
  _computed.set(static_cast<size_t>(s));
}

void Stats::set_computed(Stat s, bool flag) {
  _computed.set(static_cast<size_t>(s), flag);
}


int64_t Stats::countna(const Column* col) {
  if (!is_computed(Stat::NaCount)) compute_countna(col);
  return _countna;
}

int64_t Stats::nunique(const Column* col) {
  if (!is_computed(Stat::NUnique)) compute_sorted_stats(col);
  return _nunique;
}

int64_t Stats::nmodal(const Column* col) {
  if (!is_computed(Stat::NModal)) compute_sorted_stats(col);
  return _nmodal;
}

void Stats::set_countna(int64_t n) {
  set_computed(Stat::NaCount, !ISNA<int64_t>(n));
  _countna = n;
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
void Stats::verify_integrity(const Column* col) const {
  auto test = std::unique_ptr<Stats>(make());
  verify_stat(Stat::NaCount, _countna, [&](){ return test->countna(col); });
  verify_stat(Stat::NUnique, _nunique, [&](){ return test->nunique(col); });
  verify_stat(Stat::NModal,  _nmodal,  [&](){ return test->nmodal(col); });
  verify_more(test.get(), col);
}

void Stats::verify_more(Stats*, const Column*) const {}


template <typename T, typename F>
void Stats::verify_stat(Stat s, T value, F getter) const {
  if (!is_computed(s)) return;
  T test_value = getter();
  if (value == test_value) return;
  if (std::is_floating_point<T>::value && (value != 0) &&
      std::abs(1.0 - static_cast<double>(test_value/value)) < 1e-14) return;
  throw AssertionError()
      << "Stored " << stat_name(s) << " stat is " << value
      << ", whereas computed " << stat_name(s) << " is " << getter();
}



//==============================================================================
// NumericalStats
//==============================================================================

/**
 * Standard deviation and mean computations are done using Welford's method.
 * Ditto for skewness and kurtosis computations.
 * (Source: https://www.johndcook.com/blog/standard_deviation)
 */
template <typename T, typename A>
void NumericalStats<T, A>::compute_numerical_stats(const Column* col) {
  int64_t nrows = col->nrows;
  const RowIndex& rowindex = col->rowindex();
  const T* data = static_cast<const T*>(col->data());
  int64_t count_notna = 0;
  double mean = 0;
  double m2 = 0;
  double m3 = 0;
  double m4 = 0;
  A sum = 0;
  T min = infinity<T>();
  T max = -infinity<T>();

  #pragma omp parallel
  {
    int ith = omp_get_thread_num();  // current thread index
    int nth = omp_get_num_threads(); // total number of threads
    int64_t t_count_notna = 0;
    int64_t n1 = 0;
    int64_t n2 = 0; // added for readability
    double t_mean = 0;
    double t_m2 = 0;
    double t_m3 = 0;
    double t_m4 = 0;
    double t_m2_helper = 0;

    A t_sum = 0;
    T t_min = infinity<T>();
    T t_max = -infinity<T>();

    rowindex.strided_loop(ith, nrows, nth,
      [&](int64_t i) {
        if (ISNA<int64_t>(i)) return;
        T x = data[i];
        if (ISNA<T>(x)) return;
        n1 = t_count_notna;
        ++t_count_notna;
        n2 = t_count_notna; // readability
        t_sum += static_cast<A>(x);
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

    #pragma omp critical
    {
      if (t_count_notna > 0) {
        int64_t nold = count_notna;
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
        int64_t a_n = nold;
        int64_t b_n = t_count_notna;
        int64_t c_n = count_notna;

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
    }
  }

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


template <typename T, typename A>
void NumericalStats<T, A>::compute_sorted_stats(const Column* col) {
  const T* coldata = static_cast<const T*>(col->data());
  Groupby grpby;
  RowIndex ri = col->sort(&grpby);
  const int32_t* groups = grpby.offsets_r();
  size_t n_groups = grpby.ngroups();

  // Sorting gathers all NA elements at the top (in the first group). Thus if
  // we did not yet compute the NA count for the column, we can do so now by
  // checking whether the elements in the first group are NA or not.
  if (!is_computed(Stat::NaCount)) {
    T x0 = coldata[ri.nth(0)];
    _countna = ISNA<T>(x0)? groups[1] : 0;
    set_computed(Stat::NaCount);
  }

  bool has_nas = (_countna > 0);
  _nunique = static_cast<int64_t>(n_groups) - has_nas;
  set_computed(Stat::NUnique);

  int64_t max_grpsize = 0;
  size_t best_igrp = 0;
  for (size_t i = has_nas; i < n_groups; ++i) {
    int32_t grpsize = groups[i + 1] - groups[i];
    if (grpsize > max_grpsize) {
      max_grpsize = grpsize;
      best_igrp = i;
    }
  }

  _nmodal = max_grpsize;
  _mode = max_grpsize ? coldata[ri.nth(groups[best_igrp])] : GETNA<T>();
  set_computed(Stat::NModal);
  set_computed(Stat::Mode);
}


template <typename T, typename A>
A NumericalStats<T, A>::sum(const Column* col) {
  if (!is_computed(Stat::Sum)) compute_numerical_stats(col);
  return _sum;
}

template <typename T, typename A>
T NumericalStats<T, A>::min(const Column* col) {
  if (!is_computed(Stat::Min)) compute_numerical_stats(col);
  return _min;
}

template <typename T, typename A>
T NumericalStats<T, A>::max(const Column* col) {
  if (!is_computed(Stat::Max)) compute_numerical_stats(col);
  return _max;
}

template <typename T, typename A>
T NumericalStats<T, A>::mode(const Column* col) {
  if (!is_computed(Stat::Mode)) compute_sorted_stats(col);
  return _mode;
}

template <typename T, typename A>
double NumericalStats<T, A>::mean(const Column* col) {
  if (!is_computed(Stat::Mean)) compute_numerical_stats(col);
  return _mean;
}

template <typename T, typename A>
double NumericalStats<T, A>::stdev(const Column* col) {
  if (!is_computed(Stat::StDev)) compute_numerical_stats(col);
  return _sd;
}

template <typename T, typename A>
double NumericalStats<T, A>::skew(const Column* col) {
  if (!is_computed(Stat::Skew)) compute_numerical_stats(col);
  return _skew;
}

template <typename T, typename A>
double NumericalStats<T, A>::kurt(const Column* col) {
  if (!is_computed(Stat::Kurt)) compute_numerical_stats(col);
  return _kurt;
}

template<typename T, typename A>
void NumericalStats<T, A>::compute_countna(const Column* col) {
  compute_numerical_stats(col);
}

template<typename T, typename A>
void NumericalStats<T, A>::set_min(T value) {
  set_computed(Stat::Min, !ISNA<T>(value));
  _min = value;
}

template<typename T, typename A>
void NumericalStats<T, A>::set_max(T value) {
  set_computed(Stat::Max, !ISNA<T>(value));
  _max = value;
}


template<typename T, typename A>
void NumericalStats<T, A>::verify_more(Stats* test, const Column* col) const
{
  auto ntest = static_cast<NumericalStats<T, A>*>(test);
  verify_stat(Stat::Min,   _min,   [&](){ return ntest->min(col); });
  verify_stat(Stat::Max,   _max,   [&](){ return ntest->max(col); });
  verify_stat(Stat::Sum,   _sum,   [&](){ return ntest->sum(col); });
  verify_stat(Stat::Mean,  _mean,  [&](){ return ntest->mean(col); });
  // verify_stat(Stat::StDev, _sd,    [&](){ return ntest->stdev(col); });
  // verify_stat(Stat::Skew,  _skew,  [&](){ return ntest->skew(col); });
  // verify_stat(Stat::Kurt,  _kurt,  [&](){ return ntest->kurt(col); });
}



template class NumericalStats<int8_t, int64_t>;
template class NumericalStats<int16_t, int64_t>;
template class NumericalStats<int32_t, int64_t>;
template class NumericalStats<int64_t, int64_t>;
template class NumericalStats<float, double>;
template class NumericalStats<double, double>;



//==============================================================================
// RealStats
//==============================================================================

// Adds a check for infinite/NaN mean and sd.
template <typename T>
void RealStats<T>::compute_numerical_stats(const Column *col) {
  NumericalStats<T, double>::compute_numerical_stats(col);
  if (std::isinf(this->_min) || std::isinf(this->_max)) {
    this->_sd = GETNA<double>();
    this->_skew = GETNA<double>();
    this->_kurt = GETNA<double>();
    this->_mean = std::isinf(this->_min) && this->_min < 0 &&
                  std::isinf(this->_max) && this->_max > 0
        ? GETNA<double>()
        : static_cast<double>(std::isinf(this->_min) ? this->_min : this->_max);
  }
}


template <typename T>
RealStats<T>* RealStats<T>::make() const {
  return new RealStats<T>();
}


template class RealStats<float>;
template class RealStats<double>;



//==============================================================================
// IntegerStats
//==============================================================================

template <typename T>
IntegerStats<T>* IntegerStats<T>::make() const {
  return new IntegerStats<T>();
}


template class IntegerStats<int8_t>;
template class IntegerStats<int16_t>;
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
void BooleanStats::compute_numerical_stats(const Column *col) {
  int64_t count0 = 0, count1 = 0;
  int64_t nrows = col->nrows;
  const int8_t* data = static_cast<const int8_t*>(col->data());
  const RowIndex& rowindex = col->rowindex();
  #pragma omp parallel
  {
    int ith = omp_get_thread_num();  // current thread index
    int nth = omp_get_num_threads(); // total number of threads
    size_t tcount0 = 0, tcount1 = 0;

    rowindex.strided_loop(ith, nrows, nth,
      [&](int64_t i) {
        int8_t x = data[i];
        tcount0 += (x == 0);
        tcount1 += (x == 1);
      });

    #pragma omp critical
    {
      count0 += tcount0;
      count1 += tcount1;
    }
  }
  int64_t t_count = count0 + count1;
  double dcount0 = static_cast<double>(count0);
  double dcount1 = static_cast<double>(count1);
  _mean = t_count > 0 ? dcount1 / t_count : GETNA<double>();
  _sd = t_count > 1 ? std::sqrt(dcount0/t_count * dcount1/(t_count - 1))
                    : t_count == 1 ? 0 : GETNA<double>();
  _min = count0 ? 0 : count1 ? 1 : GETNA<int8_t>();
  _max = count1 ? 1 : count0 ? 0 : GETNA<int8_t>();
  _sum = count1;
  _countna = nrows - t_count;
  _nunique = (!!count0) + (!!count1);
  _mode = _nunique ? (count1 >= count0) : GETNA<int8_t>();
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


BooleanStats* BooleanStats::make() const {
  return new BooleanStats();
}



//==============================================================================
// StringStats
//==============================================================================

template <typename T>
void StringStats<T>::compute_countna(const Column* col) {
  const StringColumn<T>* scol = static_cast<const StringColumn<T>*>(col);
  const RowIndex& rowindex = col->rowindex();
  int64_t nrows = scol->nrows;
  int64_t countna = 0;
  const T* data = scol->offsets();

  #pragma omp parallel
  {
    int ith = omp_get_thread_num();  // current thread index
    int nth = omp_get_num_threads(); // total number of threads
    size_t tcountna = 0;

    rowindex.strided_loop(ith, nrows, nth,
      [&](int64_t i) {
        tcountna += data[i] >> (sizeof(T)*8 - 1);
      });

    #pragma omp critical
    {
      countna += tcountna;
    }
  }

  _countna = countna;
  set_computed(Stat::NaCount);
}


template <typename T>
void StringStats<T>::compute_sorted_stats(const Column* col) {
  const StringColumn<T>* scol = static_cast<const StringColumn<T>*>(col);
  const T* offsets = scol->offsets();
  Groupby grpby;
  RowIndex ri = col->sort(&grpby);
  const int32_t* groups = grpby.offsets_r();
  size_t n_groups = grpby.ngroups();

  if (!is_computed(Stat::NaCount)) {
    T off0 = offsets[ri.nth(0)];
    _countna = ISNA<T>(off0)? groups[1] : 0;
    set_computed(Stat::NaCount);
  }

  bool has_nas = (_countna > 0);
  _nunique = static_cast<int64_t>(n_groups) - has_nas;
  set_computed(Stat::NUnique);

  int64_t max_grpsize = 0;
  size_t best_igrp = 0;
  for (size_t i = has_nas; i < n_groups; ++i) {
    int32_t grpsize = groups[i + 1] - groups[i];
    if (grpsize > max_grpsize) {
      max_grpsize = grpsize;
      best_igrp = i;
    }
  }

  if (max_grpsize) {
    int64_t i = ri.nth(groups[best_igrp]);
    T o0 = offsets[i - 1] & ~GETNA<T>();
    _nmodal = max_grpsize;
    // FIXME: this is dangerous, what if strdata() pointer changes for any reason?
    _mode.ch = scol->strdata() + o0;
    _mode.size = static_cast<int64_t>(offsets[i] - o0);
  } else {
    _nmodal = 0;
    _mode.ch = nullptr;
    _mode.size = -1;
  }
  set_computed(Stat::NModal);
  set_computed(Stat::Mode);
}


template <typename T>
CString StringStats<T>::mode(const Column* col) {
  if (!is_computed(Stat::Mode)) compute_sorted_stats(col);
  return _mode;
}


template <typename T>
StringStats<T>* StringStats<T>::make() const {
  return new StringStats<T>();
}


template class StringStats<uint32_t>;
template class StringStats<uint64_t>;




//==============================================================================
// PyObjectStats
//==============================================================================

void PyObjectStats::compute_countna(const Column* col) {
  const RowIndex& rowindex = col->rowindex();
  int64_t nrows = col->nrows;
  int64_t countna = 0;
  PyObject* const* data = static_cast<PyObject* const*>(col->data());

  #pragma omp parallel
  {
    int ith = omp_get_thread_num();  // current thread index
    int nth = omp_get_num_threads(); // total number of threads
    size_t tcountna = 0;

    rowindex.strided_loop(ith, nrows, nth,
      [&](int64_t i) {
        tcountna += (data[i] == Py_None);
      });

    #pragma omp critical
    {
      countna += tcountna;
    }
  }

  _countna = countna;
  set_computed(Stat::NaCount);
}


void PyObjectStats::compute_sorted_stats(const Column*) {
  throw NotImplError();
}

PyObjectStats* PyObjectStats::make() const {
  return new PyObjectStats();
}
