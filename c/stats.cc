/**
 * Standard deviation and mean computations are done using Welford's method.
 * (Source: https://www.johndcook.com/blog/standard_deviation)
 *
 * ...Except in the case of booleans, where standard deviation can be
 *    derived from `count0` and `count1`:
 *            /              count0 * count1           \ 1/2
 *      sd = ( ---------------------------------------- )
 *            \ (count0 + count1 - 1)(count0 + count1) /
 */


/**
 * The Stats class has an inheritance tree shown in the picture below.
 *
 * The Stats class itself is considered to be an 'invalid' class; it will always
 * return a NA value for any requested statistic.
 * All unimplemented STypes are currently associated with the Stats class.
 *
 * NumericalStats acts as a base class for all numerical STypes.
 * IntegerStats are used for I1I, I2I, I4I, and I8I.
 * BooleanStats are used for I1B.
 * RealStats are used for F4R and F8R.
 *
 *                      +-------+
 *                      | Stats |
 *                      +-------+
 *                          |
 *                 +----------------+
 *                 | NumericalStats |
 *                 +----------------+
 *                   /           \
 *         +--------------+     +-----------+
 *         | IntegerStats |     | RealStats |
 *         +--------------+     +-----------+
 *              /
 *     +--------------+
 *     | BooleanStats |
 *     +--------------+
 */
#include <cmath>
#include "utils.h"
#include "stats.h"
#include "column.h"
#include "rowindex.h"

/**
 * Macro to get column indices based on a RowIndex `RI`.
 * Creates a for loop with from the following parameters:
 *
 *     `RI`:    A RowIndex pointer.
 *
 *     `NROWS`: The length of the target column.
 *
 *     `I`:     A variable name that will be initialized as an `int64_t` and
 *              used to store the resulting index during each pass.
 *
 *     `CODE`:  The code to be placed in the body of the for loop.
 *
 * Two variables named `L_j` and `L_s` will also be created; Their
 * resulting type and value is undefined.
 */
#define LOOP_OVER_ROWINDEX(I, NROWS, RI, CODE)                                 \
    if (RI == nullptr) {                                                       \
        for (int64_t I = 0; I < NROWS; ++I) {                                  \
            CODE                                                               \
        }                                                                      \
    } else if (RI->type == RI_SLICE) {                                         \
        int64_t I = RI->slice.start, L_s = RI->slice.step, L_j = 0;            \
        for (; L_j < NROWS; ++L_j, I += L_s) {                                 \
            CODE                                                               \
        }                                                                      \
    } else if (RI->type == RI_ARR32) {                                         \
       int32_t *L_s = RI->ind32;                                               \
       for (int64_t L_j = 0; L_j < NROWS; ++L_j) {                             \
           int64_t I = (int64_t) L_s[L_j];                                     \
           CODE                                                                \
       }                                                                       \
    } else if (RI->type == RI_ARR64) {                                         \
        int64_t *L_s = RI->ind64;                                              \
        for (int64_t L_j = 0; L_j < NROWS; ++L_j) {                            \
           int64_t I = L_s[L_j];                                               \
           CODE                                                                \
       }                                                                       \
    }                                                                          \

/**
 * The CStat enum is primarily used as a bit mask for checking if a statistic
 * has been computed. Note that the values of each CStat are not consecutive and
 * therefore should not be used for array maps.
 */
typedef enum CStat : uint64_t{
    C_MIN      = ((uint64_t) 1 << 0),
    C_MAX      = ((uint64_t) 1 << 1),
    C_SUM      = ((uint64_t) 1 << 2),
    C_MEAN     = ((uint64_t) 1 << 3),
    C_SD  = ((uint64_t) 1 << 4),
    C_COUNTNA = ((uint64_t) 1 << 5),
} CStat;

//==============================================================================
// Stats
//==============================================================================

Stats::Stats(const Column* col) :
  _ref_col(col),
  compute_mask(0xFFFFFFFFFFFFFFFF),
  _countna(GETNA<int64_t>()) {
  compute_mask &= ~C_COUNTNA;
}

Stats::~Stats() {}


void Stats::reset() {
  compute_mask = 0xFFFFFFFFFFFFFFFF;
  compute_mask &= ~C_COUNTNA;
}

bool Stats::countna_computed() const {
  return ((compute_mask & C_COUNTNA) != 0);
}


size_t Stats::alloc_size() const {
    return sizeof(*this);
}


// TODO: implement
void Stats::merge_stats(const Stats*) {
}



/**
 * See DataTable::verify_integrity for method description
 */
bool Stats::verify_integrity(IntegrityCheckContext&,
                             const std::string&) const
{
  // TODO: add checks (necessary when merge_stats is implemented)
  return true;
}



//==============================================================================
// NumericalStats
//==============================================================================


template <typename T, typename A>
void NumericalStats<T, A>::reset() {
  Stats::reset();
  _min     = GETNA<T>();
  _max     = GETNA<T>();
  _sum     = GETNA<A>();
  _mean    = GETNA<double>();
  _sd      = GETNA<double>();
  compute_mask &= ~(C_MIN | C_MAX | C_SUM | C_MEAN | C_SD);
}


template<typename T, typename A>
NumericalStats<T, A>::NumericalStats(const Column *col) :
  Stats(col),
  _mean(GETNA<double>()),
  _sd  (GETNA<double>()),
  _sum (GETNA<A>()),
  _min (GETNA<T>()),
  _max (GETNA<T>())
  {
  compute_mask &= ~(C_MIN | C_MAX | C_SUM | C_MEAN | C_SD);
}


template<typename T, typename A>
void NumericalStats<T, A>::compute_min() { compute_numerical_stats(); }

template<typename T, typename A>
void NumericalStats<T, A>::compute_max() { compute_numerical_stats(); }

template<typename T, typename A>
void NumericalStats<T, A>::compute_sum() { compute_numerical_stats(); }

template<typename T, typename A>
void NumericalStats<T, A>::compute_mean() { compute_numerical_stats(); }

template<typename T, typename A>
void NumericalStats<T, A>::compute_sd() { compute_numerical_stats(); }

template<typename T, typename A>
void NumericalStats<T, A>::compute_countna() { compute_numerical_stats(); }


template<typename T, typename A>
bool NumericalStats<T, A>::mean_computed() const {
  return ((compute_mask & C_MEAN) != 0);
}

template<typename T, typename A>
bool NumericalStats<T, A>::sd_computed() const {
  return ((compute_mask & C_SD) != 0);
}

template<typename T, typename A>
bool NumericalStats<T, A>::min_computed() const {
  return ((compute_mask & C_MIN) != 0);
}

template<typename T, typename A>
bool NumericalStats<T, A>::max_computed() const {
  return ((compute_mask & C_MAX) != 0);
}

template<typename T, typename A>
bool NumericalStats<T, A>::sum_computed() const {
  return ((compute_mask & C_SUM) != 0);
}
//---------------------------------------------------


// This method should be compatible with all numerical STypes.
template <typename T, typename A>
void NumericalStats<T, A>::compute_numerical_stats() {
    T t_min = GETNA<T>();
    T t_max = GETNA<T>();
    A t_sum = 0;
    double t_mean = GETNA<double>();
    double t_var  = 0;
    int64_t t_count_notna = 0;
    int64_t t_nrows = column()->nrows;
    T *data = (T*) column()->data();
    LOOP_OVER_ROWINDEX(i, t_nrows, column()->rowindex(),
        T val = data[i];
        if (ISNA(val)) continue;
        ++t_count_notna;
        t_sum += (A) val;
        if (ISNA(t_min)) {
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
    compute_mask |= C_MIN | C_MAX | C_SUM | C_MEAN | C_SD | C_COUNTNA;
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
void RealStats<T>::compute_numerical_stats() {
    NumericalStats<T, double>::compute_numerical_stats();
    if (isinf(this->_min) || isinf(this->_max)) {
        this->_sd = GETNA<double>();
        this->_mean = isinf(this->_min) && this->_min < 0 && isinf(this->_max) && this->_max > 0
                ? GETNA<double>() : (double)(isinf(this->_min) ? this->_min : this->_max);
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
// IntegerStats
//==============================================================================

void BooleanStats::compute_numerical_stats() {
    int64_t t_count0 = 0,
            t_count1 = 0;
    int8_t *data = (int8_t*) column()->data();
    int64_t nrows = column()->nrows;
    LOOP_OVER_ROWINDEX(i, nrows, column()->rowindex(),
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
    _sd = t_count > 1 ? sqrt((double)t_count0/t_count * t_count1/(t_count - 1)) :
                t_count == 1 ? 0 : GETNA<double>();
    _min = GETNA<int8_t>();
    if (t_count0 > 0) _min = 0;
    else if (t_count1 > 0) _min = 1;
    _max = !ISNA<int8_t>(_min) ? t_count1 > 0 : GETNA<int8_t>();
    _sum = t_count1;
    _countna = nrows - t_count;
    compute_mask |= C_MIN | C_MAX | C_SUM | C_MEAN | C_SD | C_COUNTNA;
}

template <typename T>
StringStats<T>::StringStats(const Column *col) :
  Stats(col) {}


template <typename T>
void StringStats<T>::reset() {
  Stats::reset();
}

template <typename T>
void StringStats<T>::compute_countna() {
  const StringColumn<T>* col = static_cast<const StringColumn<T>*>(column());
  int64_t nrows = col->nrows;
  int64_t t_countna = 0;
  T *data = col->offsets();
  LOOP_OVER_ROWINDEX(i, nrows, col->rowindex(),
      t_countna += data[i] < 0;
  )
  _countna = t_countna;
  compute_mask |= C_COUNTNA;
}

template class StringStats<int32_t>;
template class StringStats<int64_t>;

//==============================================================================
// Helper Functions
//==============================================================================

/**
 * Initialize a Stats structure
 */
/*Stats* Stats::construct(const Column *col) {
    if (col == nullptr) return new Stats(nullptr);
    switch(col->stype()) {
    case ST_BOOLEAN_I1:
        return new BooleanStats(col);
    case ST_INTEGER_I1:
        return new IntegerStats<int8_t>(col);
    case ST_INTEGER_I2:
        return new IntegerStats<int16_t>(col);
    case ST_INTEGER_I4:
        return new IntegerStats<int32_t>(col);
    case ST_INTEGER_I8:
        return new IntegerStats<int64_t>(col);
    case ST_REAL_F4:
        return new RealStats<float>(col);
    case ST_REAL_F8:
        return new RealStats<double>(col);
    default:
        return new Stats(col);
    }
}*/



/**
 * Create a column that contains a single NA value.
 */
/*Column* make_na_stat_column(SType stype) {
    Column *out = Column::new_data_column(stype, 1);
    const void *val = stype_info[stype].na;
    if (!val)
        memset(out->data(), 0xFF, out->alloc_size());
    else
        memcpy(out->data(), val, stype_info[stype].elemsize);
    return out;
}*/


void init_stats(void) {
}
