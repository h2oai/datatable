//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "stats.h"
#include <cmath>     // std::isinf, std::sqrt
#include <limits>    // std::numeric_limits
#include "column.h"
#include "rowindex.h"
#include "utils.h"
#include "utils/omp.h"


/**
 * The CStat enum is primarily used as a bit mask for checking if a statistic
 * has been computed. Note that the values of each CStat are not consecutive and
 * therefore should not be used for array maps.
 */
enum Mask : uint64_t {
  MIN      = (1ULL << 0),
  MAX      = (1ULL << 1),
  SUM      = (1ULL << 2),
  MEAN     = (1ULL << 3),
  SD       = (1ULL << 4),
  COUNTNA  = (1ULL << 5),
  NUNIQ    = (1ULL << 6),
};

template<typename T>
constexpr T infinity() {
  return std::numeric_limits<T>::has_infinity
         ? std::numeric_limits<T>::infinity()
         : std::numeric_limits<T>::max();
}



//==============================================================================
// Stats
//==============================================================================

void Stats::reset() {
  _computed.reset();
}

bool Stats::is_computed(Stat s) const {
  return _computed.test(s);
}

int64_t Stats::countna(const Column* col) {
  if (!_computed.test(Stat::NaCnt)) compute_countna(col);
  return _countna;
}


size_t Stats::memory_footprint() const {
  return sizeof(*this);
}


// TODO: implement
void Stats::merge_stats(const Stats*) {
}


/**
 * See DataTable::verify_integrity for method description
 */
// TODO: implement
bool Stats::verify_integrity(IntegrityCheckContext&, const std::string&) const {
  return true;
}



//==============================================================================
// NumericalStats
//==============================================================================

/**
 * Standard deviation and mean computations are done using Welford's method.
 * (Source: https://www.johndcook.com/blog/standard_deviation)
 */
template <typename T, typename A>
void NumericalStats<T, A>::compute_numerical_stats(const Column* col) {
  int64_t nrows = col->nrows;
  const RowIndex& rowindex = col->rowindex();
  T* data = static_cast<T*>(col->data());
  int64_t count_notna = 0;
  double mean = 0;
  double m2 = 0;
  A sum = 0;
  T min = infinity<T>();
  T max = -infinity<T>();

  #pragma omp parallel
  {
    int ith = omp_get_thread_num();  // current thread index
    int nth = omp_get_num_threads(); // total number of threads
    int64_t t_count_notna = 0;
    double t_mean = 0;
    double t_m2 = 0;
    A t_sum = 0;
    T t_min = infinity<T>();
    T t_max = -infinity<T>();

    rowindex.strided_loop(ith, nrows, nth,
      [&](int64_t i) {
        T x = data[i];
        if (ISNA<T>(x)) return;
        ++t_count_notna;
        t_sum += static_cast<A>(x);
        if (x < t_min) t_min = x;  // Note: these ifs are not exclusive!
        if (x > t_max) t_max = x;
        double delta = static_cast<double>(x) - t_mean;
        t_mean += delta / t_count_notna;
        double delta2 = static_cast<double>(x) - t_mean;
        t_m2 += delta * delta2;
      });

    #pragma omp critical
    {
      if (t_count_notna > 0) {
        double nold = static_cast<double>(count_notna);
        count_notna += t_count_notna;
        sum += t_sum;
        if (t_min < min) min = t_min;
        if (t_max > max) max = t_max;
        double delta = mean - t_mean;
        m2 += t_m2 + delta * delta * (nold / count_notna * t_count_notna);
        mean = static_cast<double>(sum) / count_notna;
      }
    }
  }

  _countna = nrows - count_notna;
  if (count_notna == 0) {
    _min = _max = GETNA<T>();
    _mean = _sd = GETNA<double>();
    _sum = 0;
  } else {
    _min = min;
    _max = max;
    _sum = sum;
    _mean = mean;
    _sd = count_notna > 1 ? std::sqrt(m2 / (count_notna - 1)) : 0;
  }
  _computed.set(Stat::Min);
  _computed.set(Stat::Max);
  _computed.set(Stat::Sum);
  _computed.set(Stat::Mean);
  _computed.set(Stat::StDev);
  _computed.set(Stat::NaCnt);
}


template <typename T, typename A>
void NumericalStats<T, A>::compute_sorted_stats(const Column* col) {
  RowIndex ri = col->sort(true);
  _nuniq = static_cast<int64_t>(ri.get_ngroups());
}


template <typename T, typename A>
A NumericalStats<T, A>::sum(const Column* col) {
  if (!_computed.test(Stat::Sum)) compute_numerical_stats(col);
  return _sum;
}

template <typename T, typename A>
T NumericalStats<T, A>::min(const Column* col) {
  if (!_computed.test(Stat::Min)) compute_numerical_stats(col);
  return _min;
}

template <typename T, typename A>
T NumericalStats<T, A>::max(const Column* col) {
  if (!_computed.test(Stat::Max)) compute_numerical_stats(col);
  return _max;
}

template <typename T, typename A>
double NumericalStats<T, A>::mean(const Column* col) {
  if (!_computed.test(Stat::Mean)) compute_numerical_stats(col);
  return _mean;
}

template <typename T, typename A>
double NumericalStats<T, A>::stdev(const Column* col) {
  if (!_computed.test(Stat::StDev)) compute_numerical_stats(col);
  return _sd;
}

template<typename T, typename A>
void NumericalStats<T, A>::compute_countna(const Column* col) {
  compute_numerical_stats(col);
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
    this->_mean = std::isinf(this->_min) && this->_min < 0 &&
                  std::isinf(this->_max) && this->_max > 0
        ? GETNA<double>()
        : static_cast<double>(std::isinf(this->_min) ? this->_min : this->_max);
  }
}


template class RealStats<float>;
template class RealStats<double>;



//==============================================================================
// IntegerStats
//==============================================================================

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
  int8_t* data = static_cast<int8_t*>(col->data());
  int64_t nrows = col->nrows;
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
  _computed.set(Stat::Min);
  _computed.set(Stat::Max);
  _computed.set(Stat::Sum);
  _computed.set(Stat::Mean);
  _computed.set(Stat::StDev);
  _computed.set(Stat::NaCnt);
}



//==============================================================================
// StringStats
//==============================================================================

template <typename T>
void StringStats<T>::compute_countna(const Column *col) {
  const StringColumn<T>* scol = static_cast<const StringColumn<T>*>(col);
  const RowIndex& rowindex = col->rowindex();
  int64_t nrows = scol->nrows;
  int64_t countna = 0;
  T* data = scol->offsets();

  #pragma omp parallel
  {
    int ith = omp_get_thread_num();  // current thread index
    int nth = omp_get_num_threads(); // total number of threads
    size_t tcountna = 0;

    rowindex.strided_loop(ith, nrows, nth,
      [&](int64_t i) {
        tcountna += data[i] < 0;
      });

    #pragma omp critical
    {
      countna += tcountna;
    }
  }

  _countna = countna;
  _computed.set(Stat::NaCnt);
}


template class StringStats<int32_t>;
template class StringStats<int64_t>;
