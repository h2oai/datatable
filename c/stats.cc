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
#include <stats.h>
#include "utils.h"
#include "column.h"
#include "rowindex.h"

/**
 * Macro to get column indices based on a RowIndex `RI`.
 * Creates a for loop with from the following parameters:
 *
 *     `RI`:    A RowIndex pointer.
 *
 *     `NROWS`: The length of the target column. This variable will be
 *              modified to store the `RI`->length, if applicable.
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
    if (RI == NULL) {                                                          \
        for (int64_t I = 0; I < NROWS; ++I) {                                  \
            CODE                                                               \
        }                                                                      \
    } else {                                                                   \
        NROWS = RI->length;                                                    \
        if (RI->type == RI_SLICE) {                                            \
            int64_t I = RI->slice.start, L_s = RI->slice.step, L_j = 0;        \
            for (; L_j < NROWS; ++L_j, I += L_s) {                             \
                CODE                                                           \
            }                                                                  \
        } else if (RI->type == RI_ARR32) {                                     \
           int32_t *L_s = RI->ind32;                                           \
           for (int64_t L_j = 0; L_j < NROWS; ++L_j) {                         \
               int64_t I = (int64_t) L_s[L_j];                                 \
               CODE                                                            \
           }                                                                   \
        } else if (RI->type == RI_ARR64) {                                     \
            int64_t *L_s = RI->ind64;                                          \
            for (int64_t L_j = 0; L_j < NROWS; ++L_j) {                        \
               int64_t I = L_s[L_j];                                           \
               CODE                                                            \
           }                                                                   \
        }                                                                      \
    }

/**
 * The CStat enum is primarily used as a bit mask for checking if a statistic
 * has been computed. Note that the values of each CStat are not consecutive and
 * therefore should not be used for array maps.
 */
typedef uint64_t stat_cinfo;
typedef enum CStat : stat_cinfo {
    C_MIN      = ((stat_cinfo) 1 << 0),
    C_MAX      = ((stat_cinfo) 1 << 1),
    C_SUM      = ((stat_cinfo) 1 << 2),
    C_MEAN     = ((stat_cinfo) 1 << 3),
    C_STD_DEV  = ((stat_cinfo) 1 << 4),
    C_COUNT_NA = ((stat_cinfo) 1 << 5),
} CStat;

/**
 * VOID_PTR acts as the "null" Stats instance to which all Stats pointers should
 * refer instead of NULL. This is so that a NULL check is not needed for every
 * Stats method call.
 **/
static Stats* VOID_PTR;

/**
 * Determines if a CStat is available for retrieval. If false, the CStat should
 * be computed before exposing. Note that all invalid CStats are considered to
 * be "computed" since their values are known to be NA.
 */
static bool is_computed(const stat_cinfo, const CStat);

/**
 * Helper function that makes a single-row column with a NA value.
 */
static Column* make_na_stat_column(SType);


//==============================================================================
// Stats
//==============================================================================

Stats::Stats(const Column* col, const RowIndex* ri = NULL) {
    _ref_col = col;
    _ref_ri = ri;
    _mean = _sd = GETNA<double>();
    _countna = GETNA<int64_t>();
}


Stats::~Stats() {}


bool Stats::is_void() const { return this == VOID_PTR; }


void Stats::reset() {
    _mean = _sd = GETNA<double>();
    _countna = GETNA<int64_t>();
}


size_t Stats::alloc_size() const {
    return sizeof(*this);
}


// TODO: Implement in child classes
void Stats::merge_stats(const Stats*) {}


//------------------ Get Stat Value ------------------
double  Stats::mean()     { return _mean; }
double  Stats::sd()       { return _sd; }
int64_t Stats::countna() { return _countna; }

/** virtual template methods are not allowed in C++,
 * so these functions act as wrappers to the virtual
 * method `stat_raw()`. The methods below attempt to
 * cast the void pointer returned by `stat_raw()` to
 * the specified template type.
 */
template <typename T> T Stats::min() {
    T* val = (T*) min_raw();
    return val ? *val : GETNA<T>();
}

template <typename T> T Stats::max() {
    T* val = (T*) max_raw();
    return val ? *val : GETNA<T>();
}

template <typename A> A Stats::sum() {
    A* val = (A*) sum_raw();
    return val ? *val : GETNA<A>();
}
//----------------------------------------------------


/**
 * The `stat_raw()` methods return a reference to the value of the statistic
 * as a void pointer. See the description of the template `stat()` methods for
 * information.
 */
void* Stats::min_raw() { return NULL; }
void* Stats::max_raw() { return NULL; }
void* Stats::sum_raw() { return NULL; }


//----------- Get Stat Value as a Column ------------
Column* Stats::mean_column() {
    Column *out = Column::new_data_column(ST_REAL_F8, 1);
    *((double*)(out->data())) = mean();
    return out;
}

Column* Stats::sd_column() {
    Column *out = Column::new_data_column(ST_REAL_F8, 1);
    *((double*)(out->data())) = sd();
    return out;
}

Column* Stats::countna_column() {
    Column *out = Column::new_data_column(ST_INTEGER_I8, 1);
    *((int64_t*)(out->data())) = countna();
    return out;
}

Column* Stats::min_column() {
    return make_na_stat_column(_ref_col->stype());
}

Column* Stats::max_column() {
    return make_na_stat_column(_ref_col->stype());
}

Column* Stats::sum_column() {
    return make_na_stat_column(_ref_col->stype());
}
//---------------------------------------------------


//------------------------------- Static Methods -------------------------------

Stats* Stats::void_ptr() { return VOID_PTR; }


/**
 * Macro to define the static `stat_datatable()` methods. The `STAT` parameter
 * is the name of the statistic; exactly how it is named in the
 * `stat_datatable()` method.
 */
#define STAT_DATATABLE(STAT)                                                   \
    DataTable* Stats:: STAT ## _datatable(const DataTable* dt) {               \
        Column** cols = dt->columns;                                           \
        Column** out_cols = NULL;                                              \
        dtmalloc(out_cols, Column*, (size_t) (dt->ncols + 1));                 \
        if (dt->rowindex) {                                                    \
            Stats** stats = dt->stats;                                         \
            for (int64_t i = 0; i < dt->ncols; ++i) {                          \
                 if (stats[i]->is_void()) {                                    \
                     stats[i] = construct(cols[i], dt->rowindex);              \
                 }                                                             \
                 out_cols[i] = stats[i]-> STAT ## _column();                   \
            }                                                                  \
        } else {                                                               \
            for (int64_t i = 0; i < dt->ncols; ++i) {                          \
                Column* col = cols[i];                                         \
                if (col->stats->is_void()) {                                   \
                    col->stats = construct(col, NULL);                         \
                }                                                              \
                out_cols[i] = col->stats-> STAT ## _column();                  \
            }                                                                  \
        }                                                                      \
        out_cols[dt->ncols] = NULL;                                            \
        return new DataTable(out_cols);                                        \
    }

STAT_DATATABLE(mean)
STAT_DATATABLE(sd)
STAT_DATATABLE(countna)
STAT_DATATABLE(min)
STAT_DATATABLE(max)
STAT_DATATABLE(sum)

#undef STAT_DATATABLE

//------------------------------------------------------------------------------


//==============================================================================
// NumericalStats
//==============================================================================
/**
 * Base class for all numerical STypes. Two template types are used:
 *     T - The type for all statistics containing a value from `_ref_col`
 *         (e.g. min, max). Naturally, `T` should be compatible with `_ref_col`s
 *         SType.
 *     A - The type for all aggregate statistics whose type is dependent on `T`
 *         (e.g. sum).
 * This class itself is not compatible with any SType; one of its children
 * should be used instead.
 */
template <typename T, typename A>
class NumericalStats : public Stats {
public:
    NumericalStats(const Column*, const RowIndex*);
    double  mean()    override final { compute_cstat(C_MEAN);     return _mean; }
    double  sd()      override final { compute_cstat(C_STD_DEV);  return _sd; }
    int64_t countna() override final { compute_cstat(C_COUNT_NA); return _countna; }
    void reset() override final;
protected:
    A _sum;
    T _min;
    T _max;
    int8_t _padding[8 - ((sizeof(T) * 2) % 8)];
    stat_cinfo _computed;

    Column* min_column() override final;
    Column* max_column() override final;
    Column* sum_column() override final;

    void* min_raw() override final { compute_cstat(C_MIN); return &_min; }
    void* max_raw() override final { compute_cstat(C_MAX); return &_max; }
    void* sum_raw() override final { compute_cstat(C_SUM); return &_sum; }

    // Helper method to call the appropriate computation function for each CStat
    // (currently there is only one)
    void compute_cstat(const CStat);

    // There is no need for `stype_T()` since it should be equivalent to
    // `_ref_col->stype`
    virtual SType stype_A() { return ST_VOID; }

    // Computes min, max, sum, mean, sd, and countna
    virtual void compute_numerical_stats();
};


template <typename T, typename A>
void NumericalStats<T, A>::reset() {
    Stats::reset();
    _min = GETNA<T>();
    _max = GETNA<T>();
    _sum = GETNA<A>();
    _computed = ~((stat_cinfo) 0 | C_MIN | C_MAX | C_SUM | C_MEAN | C_STD_DEV | C_COUNT_NA);
}


template<typename T, typename A>
NumericalStats<T, A>::NumericalStats(const Column *col, const RowIndex *ri)
                              : Stats(col, ri) {
    _min = GETNA<T>();
    _max = GETNA<T>();
    _sum = GETNA<A>();
    _computed = ~((stat_cinfo) 0 | C_MIN | C_MAX | C_SUM | C_MEAN | C_STD_DEV | C_COUNT_NA);
}


//-------------- Get Stat as a Column ---------------
template <typename T, typename A>
Column* NumericalStats<T, A>::min_column() {
    const SType stype = _ref_col->stype();
    if (stype == ST_VOID) return NULL;
    Column* out = Column::new_data_column(stype, 1);
    *((T*)(out->data())) = min<T>();
    return out;
}

template <typename T, typename A>
Column* NumericalStats<T, A>::max_column() {
    const SType stype = _ref_col->stype();
    if (stype == ST_VOID) return NULL;
    Column* out = Column::new_data_column(stype, 1);
    *((T*)(out->data())) = max<T>();
    return out;
}

template <typename T, typename A>
Column* NumericalStats<T, A>::sum_column() {
    SType stype = stype_A();
    if (stype == ST_VOID) return NULL;
    Column* out = Column::new_data_column(stype, 1);
    *((A*)(out->data())) = sum<A>();
    return out;
}
//---------------------------------------------------


// Pretty redundant now, but will be more useful when categorical stats are
// implemented
template <typename T, typename A>
void NumericalStats<T, A>::compute_cstat(const CStat cstat) {
    if (is_computed(_computed, cstat)) return;
    compute_numerical_stats();
}


// This method should be compatible with all numerical STypes.
template <typename T, typename A>
void NumericalStats<T, A>::compute_numerical_stats() {
    T t_min = GETNA<T>();
    T t_max = GETNA<T>();
    A t_sum = 0;
    double t_mean = GETNA<double>();
    double t_var  = 0;
    int64_t t_count_notna = 0;
    int64_t t_nrows = _ref_col->nrows;
    T *data = (T*) _ref_col->data();
    LOOP_OVER_ROWINDEX(i, t_nrows, _ref_ri,
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
    _computed |= C_MIN | C_MAX | C_SUM |
            C_MEAN | C_STD_DEV | C_COUNT_NA;
}


//==============================================================================
// RealStats
//==============================================================================
/**
 * Child of NumericalStats that defaults `A` to a double. `T` should be a float
 * or double.
 */
template <typename T>
class RealStats : public NumericalStats<T, double> {
public:
    RealStats(const Column* col, const RowIndex* ri) :
        NumericalStats<T, double>(col, ri) {}
protected:
    SType stype_A() override final { return ST_REAL_F8; }
    void compute_numerical_stats() override;
};


// Adds a check for infinite/NaN mean and sd.
template <typename T>
void RealStats<T>::compute_numerical_stats() {
    NumericalStats<T, double>::compute_numerical_stats();
    // The this pointer is required here. Not sure why.
    if (isinf(this->_min) || isinf(this->_max)) {
        this->_sd = GETNA<double>();
        this->_mean = isinf(this->_min) && this->_min < 0 && isinf(this->_max) && this->_max > 0
                ? GETNA<double>() : (double)(isinf(this->_min) ? this->_min : this->_max);
    }
}


//==============================================================================
// IntegerStats
//==============================================================================
/**
 * Child of NumericalStats that defaults `A` to int64_t. `T` should be an
 * integer type.
 */
template <typename T>
class IntegerStats : public NumericalStats<T, int64_t> {
public:
    IntegerStats(const Column *col, const RowIndex *ri) :
        NumericalStats<T, int64_t>(col, ri) {}
protected:
    SType stype_A() override final { return ST_INTEGER_I8; }
};


//==============================================================================
// BooleanStats
//==============================================================================
/**
 * Child of IntegerStats that defaults `T` to int8_t.
 * `compute_numerical_stats()` is optimized since a boolean SType should only
 * have 3 values (true, false, NA).
 */
class BooleanStats : public IntegerStats<int8_t> {
public:
    BooleanStats(const Column*, const RowIndex*);
protected:
    void compute_numerical_stats() override;
};

BooleanStats::BooleanStats(const Column *col, const RowIndex *ri) :
                       IntegerStats<int8_t>(col, ri) {}

void BooleanStats::compute_numerical_stats() {
    int64_t t_count0 = 0,
            t_count1 = 0;
    int8_t *data = (int8_t*) _ref_col->data();
    int64_t nrows = _ref_col->nrows;
    LOOP_OVER_ROWINDEX(i, nrows, _ref_ri,
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
    _computed |= C_MIN | C_MAX | C_COUNT_NA | C_SUM | C_MEAN | C_STD_DEV;
}


//==============================================================================
// Helper Functions
//==============================================================================

/**
 * Initialize a Stats structure
 */
Stats* Stats::construct(const Column *col, const RowIndex *ri) {
    if (col == NULL) return new Stats(NULL, NULL);
    switch(col->stype()) {
        case ST_BOOLEAN_I1: return new BooleanStats(col, ri);
        case ST_INTEGER_I1: return new IntegerStats<int8_t>(col, ri);
        case ST_INTEGER_I2: return new IntegerStats<int16_t>(col, ri);
        case ST_INTEGER_I4: return new IntegerStats<int32_t>(col, ri);
        case ST_INTEGER_I8: return new IntegerStats<int64_t>(col, ri);
        case ST_REAL_F4:    return new RealStats<float>(col, ri);
        case ST_REAL_F8:    return new RealStats<double>(col, ri);
        default:            return new Stats(col, ri);
    }
}


/**
 * Destroy a Stats structure
 */
void Stats::destruct(Stats* self) {
    if (self != VOID_PTR) delete self;
}


/**
 * Create a column that contains a single NA value.
 */
Column* make_na_stat_column(SType stype) {
    Column *out = Column::new_data_column(stype, 1);
    const void *val = stype_info[stype].na;
    if (!val)
        memset(out->data(), 0xFF, out->alloc_size());
    else
        memcpy(out->data(), val, stype_info[stype].elemsize);
    return out;
}


/**
 * Check if a CStat is computed
 */

bool is_computed(const stat_cinfo info, const CStat cstat) {
    return (info & cstat) != 0;
}


void init_stats(void) {
    VOID_PTR = Stats::construct(NULL, NULL);
}
