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
 *     and being able to fit inside `int64_t`). The variable `start` is declared
 *     as `int64_t` because if it was `size_t` then it wouldn't be possible to
 *     do simple addition `start + step`.
 */
typedef struct RowMapping {
    RowMappingType type;
    int64_t length;
    union {
        int32_t *ind32;
        int64_t *ind64;
        struct { int64_t start, step; } slice;
    };
} RowMapping;



//==============================================================================
// Public API
//==============================================================================
typedef int (rowmapping_filterfn32)(int64_t, int64_t, int32_t*, int32_t*);
typedef int (rowmapping_filterfn64)(int64_t, int64_t, int64_t*, int32_t*);


/**
 * Reduce storage method of rowmapping `rwm` from RM_ARR64 to RM_ARR32, if
 * possible.
 */
_Bool rowmapping_compactify(RowMapping *rwm);

/**
 * Construct a "slice" RowMapping object from triple `(start, count, step)`.
 */
RowMapping* rowmapping_from_slice(int64_t start, int64_t count, int64_t step);

/**
 * Construct an "array" RowMapping object from a series of triples `(start,
 * count, step)`. Either RM_ARR32 or RM_ARR64 object will be constructed,
 * depending on the sizes and indices within the slicelist.
 */
RowMapping* rowmapping_from_slicelist(
    int64_t *starts, int64_t *counts, int64_t *steps, int64_t n);

/**
 * Construct a `RowMapping` object from a plain list of int64_t/int32_t row
 * indices.
 */
RowMapping* rowmapping_from_i64_array(int64_t *array, int64_t n);
RowMapping* rowmapping_from_i32_array(int32_t *array, int64_t n);

/**
 * Construct a `RowMapping` object using a boolean data column `col`.
 */
RowMapping* rowmapping_from_datacolumn(Column *col, int64_t nrows);

/**
 * Construct a `RowMapping` object using a boolean data column `col` with
 * another RowMapping applied to it.
 */
RowMapping* rowmapping_from_column_with_rowmapping(
    Column *col, RowMapping *rmp);

/**
 * Construct a `RowMapping` object using an external filter function. This
 * filter function takes a range of rows `row0:row1` and an output buffer,
 * and writes the indices of the selected rows into that buffer. The
 * `rowmapping_from_filterfn` function then handles assemnling that output
 * into final RowMapping structure, as well as distributing the work load
 * among multiple threads.
 *
 * The 32-bit version of the function should be called when `nrows` does not
 * exceed INT32_MAX, and the 64-bit version otherwise.
 *
 * @param f
 *     Pointer to the filter function with the signature `(row0, row1, out,
 *     nouts) -> int`. The filter function has to determine which rows in the
 *     range `row0:row1` test positive, and write their indices into the array
 *     `out`. It should also store in the variable `nouts` the number of rows
 *     selected.
 *
 * @param nrows
 *     Number of rows in the datatable that is being filtered.
 */
RowMapping* rowmapping_from_filterfn32(rowmapping_filterfn32 *f, int64_t nrows);
RowMapping* rowmapping_from_filterfn64(rowmapping_filterfn64 *f, int64_t nrows);

/**
 * Combine two RowMappings: `rwm_ab` + `rwm_bc` => `rwm_ac`.
 */
RowMapping* rowmapping_merge(RowMapping *rwm_ab, RowMapping *rwm_bc);

/**
 * RowMapping's destructor.
 */
void rowmapping_dealloc(RowMapping *rowmapping);


#endif
