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
#include <math.h>
#include <stats.h>
#include "utils.h"
#include "column.h"
#include "rowindex.h"

#define M_MIN      ((uint64_t) 1 << C_MIN)
#define M_MAX      ((uint64_t) 1 << C_MAX)
#define M_SUM      ((uint64_t) 1 << C_SUM)
#define M_MEAN     ((uint64_t) 1 << C_MEAN)
#define M_STD_DEV  ((uint64_t) 1 << C_STD_DEV)
#define M_COUNT_NA ((uint64_t) 1 << C_COUNT_NA)

// List of classes for every SType implemented for Stats
class I1BStats;
class I1IStats;
class I2IStats;
class I4IStats;
class I8IStats;
class F4RStats;
class F8RStats;

static SType cstat_to_stype[DT_LTYPES_COUNT][DT_CSTATS_COUNT];

Stats::Stats(const SType s) : stype(s), cstat_to_val{} {
    countna = -1;
    iscomputed = ALL_COMPUTED;
    iscomputed &= ~M_COUNT_NA;
    cstat_to_val[C_COUNT_NA] = &countna;
}

Stats::~Stats() {}

void Stats::compute_cstat(UNUSED(const Column *col), UNUSED(const RowIndex *ri), UNUSED(const CStat s)) {}

Column* Stats::make_column(const CStat s) const {
    const void* val = NULL;
    size_t val_size = 0;
    SType t_stype = cstat_to_stype[stype_info[stype].ltype][s];
    if (t_stype == ST_VOID) {
        t_stype = stype;
        val = stype_info[t_stype].na;
    } else {
        // May need to modify once more STypes are implemented
        val = cstat_to_val[s];
    }
    val_size = stype_info[t_stype].elemsize;
    Column *out = make_data_column(t_stype, 1);
    if (val) {
        memcpy(out->data, val, val_size);
    } else {
        // For ST_STRING_I(4|8)_VCHAR stypes
        memset(out->data, 0xFF, out->alloc_size);
    }
    return out;
}

void Stats::reset() {
    iscomputed = ALL_COMPUTED;
    iscomputed &= ~M_COUNT_NA;
}

bool Stats::is_computed(const CStat s) {
    return ((1 << s) & iscomputed) != 0;
}

size_t Stats::alloc_size() const {
    return sizeof(*this);
}


DataTable* make_cstat_datatable(const DataTable *self, const CStat s) {
    int64_t ncols = self->ncols;
    Column **cols = self->columns;
    Column **out_cols = NULL;
    dtcalloc(out_cols, Column*, ncols + 1);
    if (self->rowindex) {
        Stats **stats = self->stats;
        for (int64_t i = 0; i < ncols; ++i) {
            if (!stats[i]) {
                stats[i] = construct_stats(cols[i]->stype);
            }
            if (!stats[i]->is_computed(s)) {
                stats[i]->compute_cstat(cols[i], self->rowindex, s);
            }
            out_cols[i] = stats[i]->make_column(s);
        }
    } else {
        for (int64_t i = 0; i < ncols; ++i) {
            if (!cols[i]->stats) {
                cols[i]->stats = construct_stats(cols[i]->stype);
            }
            Stats *stat = cols[i]->stats;
            if (!stat->is_computed(s)) {
                stat->compute_cstat(cols[i], self->rowindex, s);
            }
            out_cols[i] = stat->make_column(s);
        }
    }
    return make_datatable(out_cols, NULL);
}

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
 * Two variables named `__TMP_0` and `__TMP_1` will also be created; Their
 * resulting type and value is undefined.
 */
#define LOOP_OVER_ROWINDEX(I, NROWS, RI, CODE)                                  \
    if (RI == NULL) {                                                           \
        for (int64_t I = 0; I < NROWS; ++I) {                                   \
            CODE                                                                \
        }                                                                       \
    } else {                                                                    \
        int64_t L_n = RI->length;                                               \
        if (RI->type == RI_SLICE) {                                             \
            int64_t I = RI->slice.start, L_s = RI->slice.step, L_j = 0;         \
            for (; L_j < L_n; L_j++, I += L_s) {                                \
                CODE                                                            \
            }                                                                   \
        } else if (RI->type == RI_ARR32) {                                      \
           int32_t *L_a = RI->ind32;                                            \
           for (int64_t L_j = 0; L_j < L_n; L_j++) {                            \
               int64_t I = (int64_t) L_a[L_j];                                  \
               CODE                                                             \
           }                                                                    \
        } else if (RI->type == RI_ARR64) {                                      \
            int64_t *L_a = RI->ind64;                                           \
            for (int64_t L_j = 0; L_j < L_n; L_j++) {                           \
               int64_t I = L_a[L_j];                                            \
               CODE                                                             \
           }                                                                    \
        }                                                                       \
    }

// FIXME: Too many macro/class definitions
#define TEMPLATE_COMPUTE_NUMERICAL_STATS(V_T, V_NA, S_T, S_NA)                              \
    void compute_num_stats(const Column* col, const RowIndex* ri) {            \
        V_T t_min = V_NA;                                                        \
        V_T t_max = V_NA;                                                        \
        S_T t_sum = 0;                                                           \
        double t_mean = NA_F8;                                                 \
        double t_var = 0;                                                      \
        int64_t t_count_notna = 0;                                             \
        int64_t t_nrows = col->nrows;                                          \
        V_T *data = (V_T*) col->data;                                              \
        LOOP_OVER_ROWINDEX(i, t_nrows, ri,                                     \
            V_T val = data[i];                                                   \
            if (IS ## V_NA (val)) continue;                                     \
            t_count_notna++;                                                   \
            t_sum += (S_T) val;                                                 \
            if (IS ## V_NA (t_min)) {                                           \
                t_min = t_max = val;                                           \
                t_mean = (double) val;                                         \
                continue;                                                      \
            }                                                                  \
            if (t_min > val) t_min = val;                                      \
            else if (t_max < val) t_max = val;                                 \
            double delta = ((double) val) - t_mean;                            \
            t_mean += delta / t_count_notna;                                   \
            double delta2 = ((double) val) - t_mean;                           \
            t_var += delta * delta2;                                           \
        )                                                                      \
        min = IS ## V_NA (t_min) ? S_NA : (S_T) t_min;                               \
        max = IS ## V_NA (t_max) ? S_NA : (S_T) t_max;                               \
        sum = t_sum;                                                           \
        mean = t_mean;                                                         \
        sd = t_count_notna > 1 ? sqrt(t_var / (t_count_notna - 1)) :             \
                     t_count_notna == 1 ? 0 : NA_F8;                           \
        if (isinf(min) || isinf(max)) {                                       \
            sd = NA_F8;                                                \
            mean = isinf(min) && min < 0 && isinf(max) && max > 0      \
                           ? NA_F8 : (double)(isinf(min) ? min : max);         \
        }           \
        countna = t_nrows - t_count_notna;                                     \
        iscomputed |= M_MIN | M_MAX | M_SUM |                                  \
                           M_MEAN | M_STD_DEV | M_COUNT_NA;                    \
    }

#define TEMPLATE_NUMERICAL_CLASS(V_T, STYPE_ABBR, LTYPE, LTYPE_CHAR, T, T_STYPE_ABBR) \
    class STYPE_ABBR ## LTYPE_CHAR ## Stats : public NumericalStats<T> {       \
    public:                                                                    \
        STYPE_ABBR ## LTYPE_CHAR ## Stats() : NumericalStats<T>(ST_ ## LTYPE ## _ ## STYPE_ABBR) {} \
    protected:                                                                 \
        TEMPLATE_COMPUTE_NUMERICAL_STATS(V_T, NA_ ## STYPE_ABBR, T, NA_ ## T_STYPE_ABBR)                      \
    };


// TODO: Restrict `T` to numerical types only
/**
 * Template class for numerical statistics. Uses two types:
 *     T - The type used to store column values (e.g. min, max).
 */
template <typename T>
class NumericalStats: public Stats {
public:
    void compute_cstat(const Column *col, const RowIndex *ri, const CStat s) override final {
        if (is_computed(s)) return;
        // TODO: Add switch/map when more stats functions are made
        compute_num_stats(col, ri);
    }
    void reset() override final {
        Stats::reset();
        iscomputed &= ~(M_MIN | M_MAX | M_SUM | M_MEAN | M_STD_DEV);
    }
protected:
    ~NumericalStats() {}
    NumericalStats(const SType val_stype) : Stats(val_stype) {
        min = max = sum = 0;
        mean = sd = 0;
        iscomputed &= ~(M_MIN | M_MAX | M_SUM | M_MEAN | M_STD_DEV);
        cstat_to_val[C_MIN] = &min;
        cstat_to_val[C_MAX] = &max;
        cstat_to_val[C_SUM] = &sum;
        cstat_to_val[C_MEAN] = &mean;
        cstat_to_val[C_STD_DEV] = &sd;
    }
    virtual void compute_num_stats(UNUSED(const Column* c), UNUSED(const RowIndex* ri)) {}
    T min, max, sum;
    double mean, sd;
};

class I1BStats: public NumericalStats<int64_t> {
public:
    I1BStats() : NumericalStats<int64_t>(ST_BOOLEAN_I1) {}
protected:
    void compute_num_stats(const Column*, const RowIndex*) override;
};

void I1BStats::compute_num_stats(const Column *col, const RowIndex *ri) {
    int64_t t_count0 = 0,
            t_count1 = 0;
    int8_t *data = (int8_t*) col->data;
    int64_t nrows = col->nrows;
    LOOP_OVER_ROWINDEX(i, nrows, ri,
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
    int64_t count = t_count0 + t_count1;
    mean = count > 0 ? ((double) t_count1) / count : NA_F8;
    sd = count > 1 ? sqrt((double)t_count0/count * t_count1/(count - 1)) :
                count == 1 ? 0 : NA_F8;
    min = NA_I1;
    if (t_count0 > 0) min = 0;
    else if (t_count1 > 0) min = 1;
    max = min != NA_I1 ? t_count1 > 0 : NA_I1;
    sum = t_count1;
    countna = col->nrows - count;
    iscomputed |= M_MIN | M_MAX | M_COUNT_NA | M_SUM | M_MEAN | M_STD_DEV;
}

TEMPLATE_NUMERICAL_CLASS(int8_t, I1, INTEGER, I, int64_t, I8)
TEMPLATE_NUMERICAL_CLASS(int16_t, I2, INTEGER, I, int64_t, I8)
TEMPLATE_NUMERICAL_CLASS(int32_t, I4, INTEGER, I, int64_t, I8)
TEMPLATE_NUMERICAL_CLASS(int64_t, I8, INTEGER, I, int64_t, I8)
TEMPLATE_NUMERICAL_CLASS(float, F4, REAL, R, double, F8)
TEMPLATE_NUMERICAL_CLASS(double, F8, REAL, R, double, F8)

/**
 * Initialize a Stats structure
 */
Stats* construct_stats(const SType stype) {
    switch(stype) {
    case ST_BOOLEAN_I1:
        return new I1BStats();
    case ST_INTEGER_I1:
        return new I1IStats();
    case ST_INTEGER_I2:
        return new I2IStats();
    case ST_INTEGER_I4:
        return new I4IStats();
    case ST_INTEGER_I8:
        return new I8IStats();
    case ST_REAL_F4:
        return new F4RStats();
    case ST_REAL_F8:
        return new F8RStats();
    default:
        return new Stats(stype);
    }
}

void init_stats(void) {
        for (int i = 0; i < DT_LTYPES_COUNT; ++i) {
        for (int j = 0; j < DT_CSTATS_COUNT; ++j) {
            cstat_to_stype[i][j] = ST_VOID;
        }
        // Global stats
        cstat_to_stype[i][C_COUNT_NA] = ST_INTEGER_I8;
    }

    cstat_to_stype[LT_BOOLEAN][C_MIN] = ST_BOOLEAN_I1;
    cstat_to_stype[LT_BOOLEAN][C_MAX] = ST_BOOLEAN_I1;
    cstat_to_stype[LT_BOOLEAN][C_MEAN] = ST_REAL_F8;
    cstat_to_stype[LT_BOOLEAN][C_STD_DEV] = ST_REAL_F8;
    cstat_to_stype[LT_BOOLEAN][C_SUM] = ST_INTEGER_I8;

    cstat_to_stype[LT_INTEGER][C_MIN] = ST_INTEGER_I8;
    cstat_to_stype[LT_INTEGER][C_MAX] = ST_INTEGER_I8;
    cstat_to_stype[LT_INTEGER][C_SUM] = ST_INTEGER_I8;
    cstat_to_stype[LT_INTEGER][C_MEAN] = ST_REAL_F8;
    cstat_to_stype[LT_INTEGER][C_STD_DEV] = ST_REAL_F8;

    cstat_to_stype[LT_REAL][C_MIN] = ST_REAL_F8;
    cstat_to_stype[LT_REAL][C_MAX] = ST_REAL_F8;
    cstat_to_stype[LT_REAL][C_SUM] = ST_REAL_F8;
    cstat_to_stype[LT_REAL][C_MEAN] = ST_REAL_F8;
    cstat_to_stype[LT_REAL][C_STD_DEV] = ST_REAL_F8;
}
