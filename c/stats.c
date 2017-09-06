/**
 * Standard deviation and mean computations are done using Welford's method.
 * Source:
 *     https://www.johndcook.com/blog/standard_deviation/
 */

#include "stats.h"
#include "utils.h"
#include <math.h>

#define M_MIN      (1 << C_MIN)
#define M_MAX      (1 << C_MAX)
#define M_MEAN     (1 << C_MEAN)
#define M_STD_DEV  (1 << C_STD_DEV)
#define M_COUNT_NA (1 << C_COUNT_NA)

typedef void (*stats_fn)(Stats*, const Column*);

static stats_fn stats_fns[DT_STYPES_COUNT];
static SType cstat_to_stype[DT_LTYPES_COUNT][DT_CSTATS_COUNT];
static uint64_t cstat_offset[DT_LTYPES_COUNT][DT_CSTATS_COUNT];

static void compute_column_cstat(Stats*, const Column*, const CStat);
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
            compute_column_cstat(stats[i], cols[i], s);
    }
}

/**
 * Compute the value of a CStat for a Stats structure given a Column.
 * Perform the computation regardless if the CStat is defined or not. Do nothing
 * if the Stats reference is NULL or the CStat/LType pairing is invalid.
 */
void compute_column_cstat(Stats *stats, const Column *column, const CStat s) {
    if (stats == NULL) return;
    if (cstat_to_stype[stype_info[stats->stype].ltype][s] == ST_VOID) return;
    // TODO: Add switch statement when multiple stats functions exist
    stats_fns[stats->stype](stats, column);
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
            compute_column_cstat(stats[i], self->columns[i], s);
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
    memcpy(out->data, val, val_size);
    return out;
}



/**
 * LT_BOOLEAN
 */
static void get_stats_i1b(Stats* self, Column* col) {
    int64_t count0 = 0,
            count1 = 0;
    int64_t countna = 0;
    int8_t *data = (int8_t*) col->data;
    for (int64_t i = 0; i < col->nrows; ++i) {
        switch (data[i]) {
            case 0 :
                ++count0;
                break;
            case 1 :
                ++count1;
                break;
            default :
                ++countna;
                break;
        };
    }
    self->b.min = NA_I1;
    if (count0 > 0) self->b.min = 0;
    else if (count1 > 0) self->b.min = 1;
    self->b.max = self->b.min != NA_I1 ? count1 > 0 : NA_I1;
    self->countna = countna;
    self->isdefined |= M_MIN | M_MAX | M_COUNT_NA;
}



/**
 * LT_INTEGER
 */
#define TEMPLATE_COMPUTE_INTEGER_STATS(stype, T, NA)                           \
    static void get_stats_ ## stype(Stats* self, Column* col) {                \
        T min = NA;                                                            \
        T max = NA;                                                            \
        double mean = NA_F8;                                                   \
        double var = 0;                                                        \
        int64_t count_notna = 0;                                               \
        int64_t nrows = col->nrows;                                            \
        T *data = (T*) col->data;                                              \
        for (int64_t i = 0; i < nrows; i++) {                                  \
            T val = data[i];                                                   \
            if (IS ## NA(val)) continue;                                       \
            count_notna++;                                                     \
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
        }                                                                      \
        self->i.min = min;                                                     \
        self->i.max = max;                                                     \
        self->i.mean = mean;                                                   \
        self->i.sd = count_notna > 1 ? sqrt(var / (count_notna - 1)) :         \
                     count_notna == 1 ? 0 : NA;                                \
        self->countna = nrows - count_notna;                                   \
        self->isdefined |= M_MIN | M_MAX | M_MEAN | M_STD_DEV | M_COUNT_NA;    \
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
    static void get_stats_ ## stype(Stats* self, Column* col) {                \
        T min = NA;                                                            \
        T max = NA;                                                            \
        double mean = NA_F8;                                                   \
        double var = 0;                                                        \
        int64_t count_notna = 0;                                               \
        int64_t nrows = col->nrows;                                            \
        T *data = (T*) col->data;                                              \
        for (int64_t i = 0; i < nrows; i++) {                                  \
            T val = data[i];                                                   \
            if (IS ## NA(val)) continue;                                       \
            count_notna++;                                                     \
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
        }                                                                      \
        self->r.min = (double) min;                                            \
        self->r.max = (double) max;                                            \
        self->r.mean = mean;                                                   \
        self->r.sd = count_notna > 1 ? sqrt(var / (count_notna - 1)) :         \
                     count_notna == 1 ? 0 : NA;                                \
        self->countna = nrows - count_notna;                                   \
        if (isinf(min) || isinf(max)) {                                        \
            self->r.sd = NAN;                                                  \
            self->r.mean = isinf(min) && min < 0 && isinf(max) && max > 0      \
                           ? NAN : (isinf(min) ? min : max);                   \
        }                                                                      \
        self->isdefined |= M_MIN | M_MAX | M_MEAN | M_STD_DEV | M_COUNT_NA;    \
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

    cstat_to_stype[LT_INTEGER][C_MIN] = ST_INTEGER_I8;
    cstat_to_stype[LT_INTEGER][C_MAX] = ST_INTEGER_I8;
    cstat_to_stype[LT_INTEGER][C_MEAN] = ST_REAL_F8;
    cstat_to_stype[LT_INTEGER][C_STD_DEV] = ST_REAL_F8;

    cstat_to_stype[LT_REAL][C_MIN] = ST_REAL_F8;
    cstat_to_stype[LT_REAL][C_MAX] = ST_REAL_F8;
    cstat_to_stype[LT_REAL][C_MEAN] = ST_REAL_F8;
    cstat_to_stype[LT_REAL][C_STD_DEV] = ST_REAL_F8;


    cstat_offset[LT_BOOLEAN][C_MIN] = offsetof(Stats, b.min);
    cstat_offset[LT_BOOLEAN][C_MAX] = offsetof(Stats, b.max);

    cstat_offset[LT_INTEGER][C_MIN] = offsetof(Stats, i.min);
    cstat_offset[LT_INTEGER][C_MAX] = offsetof(Stats, i.max);
    cstat_offset[LT_INTEGER][C_MEAN] = offsetof(Stats, i.mean);
    cstat_offset[LT_INTEGER][C_STD_DEV] = offsetof(Stats, i.sd);

    cstat_offset[LT_REAL][C_MIN] = offsetof(Stats, r.min);
    cstat_offset[LT_REAL][C_MAX] = offsetof(Stats, r.max);
    cstat_offset[LT_REAL][C_MEAN] = offsetof(Stats, r.mean);
    cstat_offset[LT_REAL][C_STD_DEV] = offsetof(Stats, r.sd);
}
