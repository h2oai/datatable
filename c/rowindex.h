#ifndef dt_ROWINDEX_H
#define dt_ROWINDEX_H
#include "column.h"
#include "datatable.h"


//==============================================================================

typedef enum RowIndexType {
    RI_ARR32 = 1,
    RI_ARR64 = 2,
    RI_SLICE = 3,
} RowIndexType;


/**
 * type
 *     The type of the RowIndex: either array or a slice. Two kinds of arrays
 *     are supported: one with 32-bit indices and having no more than 2**31-1
 *     elements, and the other with 64-bit indices and having up to 2**63-1
 *     items.
 *
 * length
 *     Total number of elements in the RowIndex (corresponds to `nrows` in
 *     the datatable). This length must be nonnegative and cannot exceed
 *     2**63-1.
 *
 * min, max
 *     The smallest / largest elements in the current RowIndex. If the length
 *     of the rowindex is 0, then both min and max will be 0.
 *
 * ind32
 *     The array of (32-bit) indices comprising the RowIndex. The number of
 *     elements in this array is `length`. Available if `type == RI_ARR32`.
 *
 * ind64
 *     The array of (64-bit) indices comprising the RowIndex. The number of
 *     elements in this array is `length`. Available if `type == RI_ARR64`.
 *
 * slice.start
 * slice.step
 *     RowIndex specified as a slice (available if `type == RI_SLICE`). The
 *     indices in this RowIndex are:
 *         start, start + step, start + 2*step, ..., start + (length - 1)*step
 *     The `start` is a nonnegative number, while `step` may be positive,
 *     negative or zero (however `start + (length - 1)*step` must be nonnegative
 *     and being able to fit inside `int64_t`). The variable `start` is declared
 *     as `int64_t` because if it was `size_t` then it wouldn't be possible to
 *     do simple addition `start + step`.
 */
typedef struct RowIndex {
    int64_t length;
    int64_t min;
    int64_t max;
    union {
        int32_t *ind32;
        int64_t *ind64;
        struct { int64_t start, step; } slice;
    };
    RowIndexType type;
    int32_t _padding;
} RowIndex;



//==============================================================================
// Public API
//==============================================================================
typedef int (rowindex_filterfn32)(int64_t, int64_t, int32_t*, int32_t*);
typedef int (rowindex_filterfn64)(int64_t, int64_t, int64_t*, int32_t*);
typedef RowIndex* (rowindex_getterfn)(void);

RowIndex* rowindex_from_slice(int64_t start, int64_t count, int64_t step);
RowIndex* rowindex_from_slicelist(
    int64_t *starts, int64_t *counts, int64_t *steps, int64_t n);
RowIndex* rowindex_from_i64_array(int64_t *array, int64_t n, int issorted);
RowIndex* rowindex_from_i32_array(int32_t *array, int64_t n, int issorted);
RowIndex* rowindex_from_boolcolumn(Column *col, int64_t nrows);
RowIndex* rowindex_from_boolcolumn_with_rowindex(Column *col, RowIndex *ri);
RowIndex* rowindex_from_intcolumn(Column *col, int is_temp_col);
RowIndex* rowindex_from_intcolumn_with_rowindex(Column *col, RowIndex *ri);
RowIndex* rowindex_from_filterfn32(rowindex_filterfn32 *f, int64_t nrows,
                                   int issorted);
RowIndex* rowindex_from_filterfn64(rowindex_filterfn64 *f, int64_t nrows,
                                   int issorted);
RowIndex* rowindex_copy(RowIndex *self);
RowIndex* rowindex_merge(RowIndex *ri_ab, RowIndex *ri_bc);
RowIndex* rowindex_expand(RowIndex *self);
void rowindex_compactify(RowIndex *self);
void rowindex_dealloc(RowIndex *self);
size_t rowindex_get_allocsize(RowIndex*);

#endif
