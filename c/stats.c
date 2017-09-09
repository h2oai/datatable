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
#include "stats.h"
#include "utils.h"
#include "rowindex.h"

#define M_MIN      (1 << C_MIN)
#define M_MAX      (1 << C_MAX)
#define M_SUM      (1 << C_SUM)
#define M_MEAN     (1 << C_MEAN)
#define M_STD_DEV  (1 << C_STD_DEV)
#define M_COUNT_NA (1 << C_COUNT_NA)

typedef void (*stats_fn)(Stats*, const Column*, const RowIndex*);

static stats_fn stats_fns[DT_STYPES_COUNT];
static SType cstat_to_stype[DT_LTYPES_COUNT][DT_CSTATS_COUNT];
static uint64_t cstat_offset[DT_LTYPES_COUNT][DT_CSTATS_COUNT];

static void compute_column_cstat(Stats*, const CStat, const Column*, const RowIndex*);
static Column* make_cstat_column(const Stats* self, const CStat);



/**
 * Initialize a Stats structure
 */
Stats* make_data_stats(const SType stype) {
    Stats *out = NULL;
    dtcalloc(out, Stats, 1);
    out->stype = stype;
    LType ltype = stype_info[stype].ltype;
    for (uint8_t i = 0; i < DT_CSTATS_COUNT; ++i) {
        if (cstat_to_stype[ltype][i] == ST_VOID)
            out->isdefined |= 1 << i;
    }
    return out;
}

/**
 * Free a Stats structure
 */
void stats_dealloc(Stats *self) {
    dtfree(self);
}

/**
 * Create a deep copy of a Stats structure
 */
Stats* stats_copy(Stats *self) {
    if (self == NULL) return NULL;
    Stats *out = NULL;
    dtmalloc(out, Stats, 1);
    memcpy(out, self, sizeof(Stats));
    return out;
}

/**
 * Determine if a CStat is defined.
 * A CStat is "defined" if...
 *     - The CStat has been computed
 * CStat is incompatible with the Stats structure's LType.
 * Return 0 if false, any other number if true.
 */
uint64_t cstat_isdefined(const Stats *self, const CStat s) {
    return self->isdefined & (1 << s);
}

/**
 * Get the total size of the memory occupied by the provided Stats structure
 */
size_t stats_get_allocsize(const Stats* self) {
    if (self == NULL) return 0;
    return sizeof(Stats);
}

/**
 * Compute the value of a CStat for each Stats structure in a DataTable.
 * Do nothing for any Stats structure that has already marked the CStat as
 * defined. Initialize a new Stats structure for any NULL reference in the
 * Stats array.
 */
void compute_datatable_cstat(DataTable *self, const CStat s) {
    Stats **stats = self->stats;
    Column **cols = self->columns;
    for (int64_t i = 0; i < self->ncols; ++i) {
        if (stats[i] == NULL)
            stats[i] = make_data_stats(cols[i]->stype);
        if (!cstat_isdefined(stats[i], s))
            compute_column_cstat(stats[i], s, cols[i], self->rowindex);
    }
}

/**
 * Compute the value of a CStat for a Stats structure given a Column.
 * Perform the computation regardless if the CStat is defined or not. Do nothing
 * if the Stats reference is NULL or the CStat/LType pairing is invalid.
 */
void compute_column_cstat(Stats *stats, const CStat s, const Column *col, const RowIndex *ri) {
    if (stats == NULL) return;
    if (cstat_to_stype[stype_info[stats->stype].ltype][s] == ST_VOID) return;
    // TODO: Add switch statement when multiple stats functions exist
    stats_fns[stats->stype](stats, col, ri);
}


/**
 * Make a DataTable with 1 row and ncols columns that contains the computed
 * CStat values from the provided DataTable's Stats array. Initialize a new
 * Stats structure for every NULL reference in the Stats array. Compute and
 * store a CStat value if it has not been defined.
 */
DataTable* make_cstat_datatable(const DataTable *self, const CStat s) {
    Stats **stats = self->stats;
    Column **out = NULL;
    dtcalloc(out, Column*, self->ncols + 1);
    for (int64_t i = 0; i < self->ncols; ++i) {
        if (stats[i] == NULL)
            stats[i] = make_data_stats(self->columns[i]->stype);
        if (!cstat_isdefined(stats[i], s))
            compute_column_cstat(stats[i], s, self->columns[i], self->rowindex);
        out[i] = make_cstat_column(stats[i], s);
    }
    return make_datatable(out, NULL);
}

/**
 * Make a 1-row Column that contains a CStat's value stored in the provided
 * Stats structure. Assume that the CStat is already defined. If the CStat
 * is incompatible with the Stats structure's LType, then create a NA Column
 * of the Stats structure's SType. Return NULL if the Stats reference is NULL.
 */
Column* make_cstat_column(const Stats *self, const CStat s) {
    if (self == NULL) return NULL;
    const void* val = NULL;
    size_t val_size = 0;
    SType stype = cstat_to_stype[stype_info[self->stype].ltype][s];
    if (stype == ST_VOID) {
        stype = self->stype;
        val = stype_info[stype].na;
        val_size = stype_info[stype].elemsize;
    } else {
        // May need to modify once more STypes are implemented
        LType ltype = stype_info[self->stype].ltype;
        val = add_constptr(self, cstat_offset[ltype][s]);
        val_size = stype_info[stype].elemsize;
    }
    Column *out = make_data_column(stype, 1);
    if (val) {
        memcpy(out->data, val, val_size);
    } else {
        // For ST_STRING_I(4|8)_VCHAR stypes
        memset(out->data, 0xFF, out->alloc_size);
    }
    return out;
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
#define FOR_ROWIDX(RI, NROWS, I, CODE)                                         \
    if (RI == NULL) {                                                           \
        for (int64_t I = 0; I < NROWS; ++I) {                                   \
            CODE                                                                \
        }                                                                       \
    } else {                                                                    \
        NROWS = RI->length;                                                     \
        if (RI->type == RI_SLICE) {                                             \
            int64_t __TMP_0 = RI->slice.step;                                   \
            for (int64_t I = RI->slice.start, __TMP_1 = 0;                      \
                 __TMP_1 < NROWS; ++__TMP_1, I += __TMP_0) {                    \
                CODE                                                            \
            }                                                                   \
        } else if (RI->type == RI_ARR32) {                                      \
           int32_t *__TMP_0 = RI->ind32;                                        \
           for(int64_t __TMP_1 = 0; __TMP_1 < NROWS; ++__TMP_1) {               \
               int64_t I = (int64_t) __TMP_0[__TMP_1];                          \
               CODE                                                             \
           }                                                                    \
        } else if (RI->type == RI_ARR64) {                                      \
            int64_t *__TMP_0 = RI->ind64;                                       \
            for(int64_t __TMP_1 = 0; __TMP_1 < NROWS; ++__TMP_1) {              \
               int64_t I = __TMP_0[__TMP_1];                                    \
               CODE                                                             \
           }                                                                    \
        }                                                                       \
    }


/**
 * LT_BOOLEAN
 */
static void get_stats_i1b(Stats* self, const Column* col, const RowIndex* ri) {
    int64_t count0 = 0,
            count1 = 0;
    int8_t *data = (int8_t*) col->data;
    int64_t nrows = col->nrows;
    FOR_ROWIDX(ri, nrows, i,
        switch (data[i]) {
            case 0 :
                ++count0;
                break;
            case 1 :
                ++count1;
                break;
            default :
                break;
        };
    )
    int64_t count = count0 + count1;
    double mean = count > 0 ? ((double) count1) / count : NA_F8;
    double sd = count > 1 ? sqrt((double)count0/count * count1/(count - 1)) :
                count == 1 ? 0 : NA_F8;
    self->b.min = NA_I1;
    if (count0 > 0) self->b.min = 0;
    else if (count1 > 0) self->b.min = 1;
    self->b.max = self->b.min != NA_I1 ? count1 > 0 : NA_I1;
    self->b.sum = count1;
    self->b.mean = mean;
    self->b.sd = count > 1 ? sd : NA_F8;
    self->countna = col->nrows - count;
    self->isdefined |= M_MIN | M_MAX | M_COUNT_NA | M_SUM | M_MEAN | M_STD_DEV;
}



/**
 * LT_INTEGER
 */
#define TEMPLATE_COMPUTE_INTEGER_STATS(stype, T, NA)                           \
    static void get_stats_ ## stype(Stats* self, const Column* col, const RowIndex* ri) {                \
        T min = NA;                                                            \
        T max = NA;                                                            \
        int64_t sum = 0;                                                       \
        double mean = NA_F8;                                                   \
        double var = 0;                                                        \
        int64_t count_notna = 0;                                               \
        int64_t nrows = col->nrows;                                            \
        T *data = (T*) col->data;                                              \
        FOR_ROWIDX(ri, nrows, i,                                               \
            T val = data[i];                                                   \
            if (IS ## NA(val)) continue;                                       \
            count_notna++;                                                     \
            sum += val;                                                        \
            if (IS ## NA(min)) {                                               \
                min = max = val;                                               \
                mean = (double) val;                                           \
                continue;                                                      \
            }                                                                  \
            if (min > val) min = val;                                          \
            else if (max < val) max = val;                                     \
            double delta = ((double) val) - mean;                              \
            mean += delta / count_notna;                                       \
            /* mean has been updated, so this is not delta**2 */               \
            var += delta * (((double) val) - mean);                            \
        )                                                                      \
        self->i.min = min;                                                     \
        self->i.max = max;                                                     \
        self->i.sum = sum;                                                     \
        self->i.mean = mean;                                                   \
        self->i.sd = count_notna > 1 ? sqrt(var / (count_notna - 1)) :         \
                     count_notna == 1 ? 0 : NA;                                \
        self->countna = nrows - count_notna;                                   \
        self->isdefined |= M_MIN | M_MAX | M_SUM |                             \
                           M_MEAN | M_STD_DEV | M_COUNT_NA;                    \
    }

TEMPLATE_COMPUTE_INTEGER_STATS(i1i, int8_t,  NA_I1)
TEMPLATE_COMPUTE_INTEGER_STATS(i2i, int16_t, NA_I2)
TEMPLATE_COMPUTE_INTEGER_STATS(i4i, int32_t, NA_I4)
TEMPLATE_COMPUTE_INTEGER_STATS(i8i, int64_t, NA_I8)
#undef TEMPLATE_COMPUTE_INTEGER_STATS


/**
 * LT_REAL
 */
#define TEMPLATE_COMPUTE_REAL_STATS(stype, T, NA)                              \
    static void get_stats_ ## stype(Stats* self, const Column* col, const RowIndex* ri) {                \
        T min = NA;                                                            \
        T max = NA;                                                            \
        double sum = 0;                                                        \
        double mean = NA_F8;                                                   \
        double var = 0;                                                        \
        int64_t count_notna = 0;                                               \
        int64_t nrows = col->nrows;                                            \
        T *data = (T*) col->data;                                              \
        FOR_ROWIDX(ri, nrows, i,                                               \
            T val = data[i];                                                   \
            if (IS ## NA(val)) continue;                                       \
            ++count_notna;                                                     \
            sum += (double) val;                                               \
            if (IS ## NA(min)) {                                               \
                min = max = val;                                               \
                mean = (double) val;                                           \
                continue;                                                      \
            }                                                                  \
            if (min > val) min = val;                                          \
            else if (max < val) max = val;                                     \
            if (isfinite(val)) {                                               \
                double delta = ((double) val) - mean;                          \
                mean += delta / count_notna;                                   \
                /* mean has been updated, so this is not delta**2 */           \
                var += delta * (((double) val) - mean);                        \
            }                                                                  \
        )                                                                      \
        self->r.min = (double) min;                                            \
        self->r.max = (double) max;                                            \
        self->r.sum = sum;                                                     \
        self->r.mean = mean;                                                   \
        self->r.sd = count_notna > 1 ? sqrt(var / (count_notna - 1)) :         \
                     count_notna == 1 ? 0 : NA_F8;                             \
        self->countna = nrows - count_notna;                                   \
        if (isinf(min) || isinf(max)) {                                        \
            /* if we ever distinguish NAs and NANs, this should be changed */  \
            self->r.sd = NA_F8;                                                \
            self->r.mean = isinf(min) && min < 0 && isinf(max) && max > 0      \
                           ? NA_F8 : (double)(isinf(min) ? min : max);         \
        }                                                                      \
        self->isdefined |= M_MIN | M_MAX | M_SUM |                             \
                           M_MEAN | M_STD_DEV | M_COUNT_NA;                    \
    }

TEMPLATE_COMPUTE_REAL_STATS(f4r, float, NA_F4)
TEMPLATE_COMPUTE_REAL_STATS(f8r, double, NA_F8)
#undef TEMPLATE_COMPUTE_REAL_STATS



/**
 * Temporary noop function.
 * Will be removed when all SType stat functions are defined.
 */
static void get_stats_noop(UNUSED(Stats *self), UNUSED(Column *col)) {}



void init_stats(void) {
    // Remove when all STypes are implemented
    for (int i = 0; i < DT_STYPES_COUNT; ++i) {
        stats_fns[i] = (stats_fn) &get_stats_noop;
    }

    stats_fns[ST_BOOLEAN_I1] = (stats_fn) &get_stats_i1b;
    stats_fns[ST_INTEGER_I1] = (stats_fn) &get_stats_i1i;
    stats_fns[ST_INTEGER_I2] = (stats_fn) &get_stats_i2i;
    stats_fns[ST_INTEGER_I4] = (stats_fn) &get_stats_i4i;
    stats_fns[ST_INTEGER_I8] = (stats_fn) &get_stats_i8i;
    stats_fns[ST_REAL_F4] = (stats_fn) &get_stats_f4r;
    stats_fns[ST_REAL_F8] = (stats_fn) &get_stats_f8r;

    for (int i = 0; i < DT_LTYPES_COUNT; ++i) {
        for (int j = 0; j < DT_CSTATS_COUNT; ++j) {
            cstat_to_stype[i][j] = ST_VOID;
            cstat_offset[i][j] = 0;
        }
        // Global stats
        cstat_to_stype[i][C_COUNT_NA] = ST_INTEGER_I8;
        cstat_offset[i][C_COUNT_NA] = offsetof(Stats, countna);
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

    // Ideally, we'd want to call `offsetof(Stats, b.min)` or similar, however
    // apparently it's not in the C standard... As a workaround, we determine
    // offsets by computing the addresses of the fields directly.
    Stats s;
    #define STATS_OFFSET(fld) ((char*)(&s.fld) - (char*)(&s))

    cstat_offset[LT_BOOLEAN][C_MIN] = STATS_OFFSET(b.min);
    cstat_offset[LT_BOOLEAN][C_MAX] = STATS_OFFSET(b.max);
    cstat_offset[LT_BOOLEAN][C_MEAN] = STATS_OFFSET(b.mean);
    cstat_offset[LT_BOOLEAN][C_STD_DEV] = STATS_OFFSET(b.sd);
    cstat_offset[LT_BOOLEAN][C_SUM] = STATS_OFFSET(b.sum);

    cstat_offset[LT_INTEGER][C_MIN] = STATS_OFFSET(i.min);
    cstat_offset[LT_INTEGER][C_MAX] = STATS_OFFSET(i.max);
    cstat_offset[LT_INTEGER][C_SUM] = STATS_OFFSET(i.sum);
    cstat_offset[LT_INTEGER][C_MEAN] = STATS_OFFSET(i.mean);
    cstat_offset[LT_INTEGER][C_STD_DEV] = STATS_OFFSET(i.sd);

    cstat_offset[LT_REAL][C_MIN] = STATS_OFFSET(r.min);
    cstat_offset[LT_REAL][C_MAX] = STATS_OFFSET(r.max);
    cstat_offset[LT_REAL][C_SUM] = STATS_OFFSET(r.sum);
    cstat_offset[LT_REAL][C_MEAN] = STATS_OFFSET(r.mean);
    cstat_offset[LT_REAL][C_STD_DEV] = STATS_OFFSET(r.sd);

    #undef STATS_OFFSET
}
