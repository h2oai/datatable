//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "stats.h"
#include <cmath>     // std::isinf
#include "column.h"
#include "rowindex.h"
#include "utils.h"


/**
 * The CStat enum is primarily used as a bit mask for checking if a statistic
 * has been computed. Note that the values of each CStat are not consecutive and
 * therefore should not be used for array maps.
 */
enum Mask : uint64_t {
  MIN      = ((uint64_t) 1 << 0),
  MAX      = ((uint64_t) 1 << 1),
  SUM      = ((uint64_t) 1 << 2),
  MEAN     = ((uint64_t) 1 << 3),
  SD       = ((uint64_t) 1 << 4),
  COUNTNA  = ((uint64_t) 1 << 5),
};



//==============================================================================
// Stats
//==============================================================================

Stats::Stats() {
  reset();
}

void Stats::reset() {
  _countna = GETNA<int64_t>();
  compute_mask = 0xFFFFFFFFFFFFFFFF & ~Mask::COUNTNA;
}

bool Stats::countna_computed() const {
  return ((compute_mask & Mask::COUNTNA) != 0);
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

template<typename T, typename A>
NumericalStats<T, A>::NumericalStats() {
  reset();
}

template <typename T, typename A>
void NumericalStats<T, A>::reset() {
  Stats::reset();
  _min  = GETNA<T>();
  _max  = GETNA<T>();
  _sum  = GETNA<A>();
  _mean = GETNA<double>();
  _sd   = GETNA<double>();
  compute_mask &= ~(Mask::MIN | Mask::MAX | Mask::SUM | Mask::MEAN | Mask::SD);
}


template<typename T, typename A> void NumericalStats<T, A>::min_compute(const Column* col)     { compute_numerical_stats(col); }
template<typename T, typename A> void NumericalStats<T, A>::max_compute(const Column* col)     { compute_numerical_stats(col); }
template<typename T, typename A> void NumericalStats<T, A>::sum_compute(const Column* col)     { compute_numerical_stats(col); }
template<typename T, typename A> void NumericalStats<T, A>::mean_compute(const Column* col)    { compute_numerical_stats(col); }
template<typename T, typename A> void NumericalStats<T, A>::sd_compute(const Column* col)      { compute_numerical_stats(col); }
template<typename T, typename A> void NumericalStats<T, A>::countna_compute(const Column* col) { compute_numerical_stats(col); }

template<typename T, typename A> bool NumericalStats<T, A>::mean_computed() const { return ((compute_mask & Mask::MEAN) != 0); }
template<typename T, typename A> bool NumericalStats<T, A>::sd_computed() const   { return ((compute_mask & Mask::SD) != 0); }
template<typename T, typename A> bool NumericalStats<T, A>::min_computed() const  { return ((compute_mask & Mask::MIN) != 0); }
template<typename T, typename A> bool NumericalStats<T, A>::max_computed() const  { return ((compute_mask & Mask::MAX) != 0); }
template<typename T, typename A> bool NumericalStats<T, A>::sum_computed() const  { return ((compute_mask & Mask::SUM) != 0); }


/**
 * Standard deviation and mean computations are done using Welford's method.
 * (Source: https://www.johndcook.com/blog/standard_deviation)
 */
template <typename T, typename A>
void NumericalStats<T, A>::compute_numerical_stats(const Column* col) {
  T t_min = GETNA<T>();
  T t_max = GETNA<T>();
  A t_sum = 0;
  double t_mean = GETNA<double>();
  double t_var  = 0;
  int64_t t_count_notna = 0;
  int64_t t_nrows = col->nrows;
  T* data = static_cast<T*>(col->data());
  DT_LOOP_OVER_ROWINDEX(i, t_nrows, col->rowindex(),
    T val = data[i];
    if (ISNA<T>(val)) continue;
    ++t_count_notna;
    t_sum += (A) val;
    if (ISNA<T>(t_min)) {
      t_min = t_max = val;
      t_mean = (double) val;
      continue;
    }
    if (t_min > val) t_min = val;
    else if (t_max < val) t_max = val;
    double delta = ((double) val) - t_mean;
    t_mean += delta / t_count_notna;
    double delta2 = ((double) val) - t_mean;
    t_var += delta * delta2;
  )
  _min = t_min;
  _max = t_max;
  _sum = t_sum;
  _mean = t_mean;
  _sd = t_count_notna > 1 ? sqrt(t_var / (t_count_notna - 1)) :
        t_count_notna == 1 ? 0 : GETNA<double>();
  _countna = t_nrows - t_count_notna;
  compute_mask |= Mask::MIN | Mask::MAX | Mask::SUM |
                  Mask::MEAN | Mask::SD | Mask::COUNTNA;
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
  int64_t t_count0 = 0,
          t_count1 = 0;
  int8_t* data = (int8_t*) col->data();
  int64_t nrows = col->nrows;
  DT_LOOP_OVER_ROWINDEX(i, nrows, col->rowindex(),
    switch (data[i]) {
      case 0 :
        ++t_count0;
        break;
      case 1 :
        ++t_count1;
        break;
      default :
        break;
    };
  )
  int64_t t_count = t_count0 + t_count1;
  _mean = t_count > 0 ? ((double) t_count1) / t_count : GETNA<double>();
  _sd = t_count > 1 ? sqrt((double)t_count0/t_count * t_count1/(t_count - 1))
                    : t_count == 1 ? 0 : GETNA<double>();
  _min = GETNA<int8_t>();
  if (t_count0 > 0) _min = 0;
  else if (t_count1 > 0) _min = 1;
  _max = !ISNA<int8_t>(_min) ? t_count1 > 0 : GETNA<int8_t>();
  _sum = t_count1;
  _countna = nrows - t_count;
  compute_mask |= Mask::MIN | Mask::MAX | Mask::SUM | Mask::MEAN | Mask::SD | Mask::COUNTNA;
}



//==============================================================================
// StringStats
//==============================================================================

template <typename T>
void StringStats<T>::countna_compute(const Column *col) {
  const StringColumn<T>* scol = static_cast<const StringColumn<T>*>(col);
  int64_t nrows = scol->nrows;
  int64_t t_countna = 0;
  T *data = scol->offsets();
  DT_LOOP_OVER_ROWINDEX(i, nrows, scol->rowindex(),
    t_countna += data[i] < 0;
  )
  _countna = t_countna;
  compute_mask |= Mask::COUNTNA;
}


template class StringStats<int32_t>;
template class StringStats<int64_t>;
