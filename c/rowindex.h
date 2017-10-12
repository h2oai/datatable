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

typedef int (rowindex_filterfn32)(int64_t, int64_t, int32_t*, int32_t*);
typedef int (rowindex_filterfn64)(int64_t, int64_t, int64_t*, int32_t*);


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
 *
 * refcount
 *     Number of references to this RowIndex object. When this count reaches 0
 *     the object can be deallocated.
 */
class RowIndex {
public:
    int64_t length;
    int64_t min;
    int64_t max;
    union {
        int32_t *ind32;
        int64_t *ind64;
        struct { int64_t start, step; } slice;
    };
    RowIndexType type;
    int32_t refcount;
    RowIndex(int64_t, int64_t, int64_t); // from slice
    RowIndex(int64_t*, int64_t*, int64_t*, int64_t); // from list of slices
    RowIndex(int64_t*, int64_t, int);
    RowIndex(int32_t*, int64_t, int);
    RowIndex(const RowIndex&);
    RowIndex(RowIndex*);
    RowIndex* expand();
    void compactify();
    size_t alloc_size();
    RowIndex* incref();
    void decref();

    static RowIndex* merge(RowIndex*, RowIndex*);
    static RowIndex* from_intcolumn(Column*, int);
    static RowIndex* from_boolcolumn(Column*, int64_t);
    static RowIndex* from_column(Column*);
    static RowIndex* from_filterfn32(rowindex_filterfn32*, int64_t, int);
    static RowIndex* from_filterfn64(rowindex_filterfn64*, int64_t, int);
private:
    ~RowIndex() {}
};



//==============================================================================
// Public API
//==============================================================================
typedef RowIndex* (rowindex_getterfn)(void);


#endif
