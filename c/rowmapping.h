#ifndef dt_ROWMAPPING_H
#define dt_ROWMAPPING_H
#include "datatable.h"


//==============================================================================

typedef enum RowMappingType {
    RM_ARR32 = 1,
    RM_ARR64 = 2,
    RM_SLICE = 3,
} RowMappingType;


/**
 * type
 *     The type of the RowMapping: either array or a slice. Two kinds of arrays
 *     are supported: one with 32-bit indices and having no more than 2**32-1
 *     elements, and the other with 64-bit indices and having up to 2**64-1
 *     items. The 64-bit array type is only available on a 64-bit platform.
 *     However it is possible (and indeed common) to create 32-bit arrays even
 *     if the platform is 64-bit.
 *
 * length
 *     Total number of elements in the RowMapping (corresponds to `nrows` in
 *     the datatable). This length must be nonnegative and cannot exceed
 *     INTPTR_MAX.
 *
 * ind32
 *     The array of (32-bit) indices comprising the RowMapping. The number of
 *     elements in this array is `length`. Available if `type == RM_ARR32`.
 *
 * ind64
 *     The array of (64-bit) indices comprising the RowMapping. The number of
 *     elements in this array is `length`. Available if `type == RM_ARR64`.
 *
 * slice.start
 * slice.step
 *     RowMapping specified as a slice (available if `type == RM_SLICE`). The
 *     indices in this RowMapping are:
 *         start, start + step, start + 2*step, ..., start + (length - 1)*step
 *     The `start` is a nonnegative number, while `step` may be positive,
 *     negative or zero (however `start + (length - 1)*step` must be nonnegative
 *     and being able to fit inside `ssize_t`). The variable `start` is declared
 *     as `ssize_t` because if it was `size_t` then it wouldn't be possible to
 *     do simple addition `start + step`.
 */
typedef struct RowMapping {
    RowMappingType type;
    ssize_t length;
    union {
        int32_t *ind32;
        int64_t *ind64;
        struct { ssize_t start, step; } slice;
    };
} RowMapping;



//==============================================================================

/**
 * Reduce storage method of rowmapping `rwm` from RM_ARR64 to RM_ARR32, if
 * possible.
 */
_Bool rowmapping_compactify(RowMapping *rwm);

/**
 * Construct a "slice" RowMapping object from triple `(start, count, step)`.
 */
RowMapping* rowmapping_from_slice(ssize_t start, ssize_t count, ssize_t step);

/**
 * Construct an "array" RowMapping object from a series of triples `(start,
 * count, step)`. Either RM_ARR32 or RM_ARR64 object will be constructed,
 * depending on the sizes and indices within the slicelist.
 */
RowMapping* rowmapping_from_slicelist(
    ssize_t *starts, ssize_t *counts, ssize_t *steps, ssize_t n);

/**
 * Construct a `RowMapping` object from a plain list of int64_t/int32_t row
 * indices.
 */
RowMapping* rowmapping_from_i64_array(int64_t *array, ssize_t n);
RowMapping* rowmapping_from_i32_array(int32_t *array, ssize_t n);

/**
 * Construct a `RowMapping` object using a boolean data column `col`.
 */
RowMapping* rowmapping_from_datacolumn(Column *col, ssize_t nrows);

/**
 * Construct a `RowMapping` object using a boolean data column `col` with
 * another RowMapping applied to it.
 */
RowMapping* rowmapping_from_column_with_rowmapping(
    Column *col, RowMapping *rmp);

/**
 * Combine two RowMappings: `rwm_ab` + `rwm_bc` => `rwm_ac`.
 */
RowMapping* rowmapping_merge(RowMapping *rwm_ab, RowMapping *rwm_bc);

/**
 * RowMapping's destructor.
 */
void rowmapping_dealloc(RowMapping *rowmapping);


#endif
