#ifndef dt_COLUMN_H
#define dt_COLUMN_H
#include <assert.h>    // static_assert
#include <inttypes.h>  // int*_t
#include <stddef.h>    // offsetof
#include "types.h"

typedef struct DataTable DataTable;
typedef struct RowMapping RowMapping;


//==============================================================================

/**
 * Single "data" column within a datatable.
 *
 * The `data` memory buffer is the raw storage of the column's data. The data
 * is stored in the "NFF" format, as described in the "types.h" file. Typically
 * it's just a plain array of primitive types, in which case the `j`-th element
 * can be accessed as `((ctype*)(column->data))[j]`. More complicated types
 * (such as strings) require a more elaborate access scheme.
 *
 * data
 *     Raw data buffer in NFF format, depending on the column's `stype` (see
 *     specification in "types.h").
 *
 * mtype
 *     "Memory" type of the column -- i.e. where the data is actually stored.
 *     Possible values are MT_DATA (column is stored in RAM) and MT_MMAP
 *     (memory-mapped from disk).
 *
 * stype
 *     Type of data within the column (the enum is defined in types.h). This is
 *     the "storage" type, i.e. how the data is actually stored in memory. The
 *     logical type can be derived from `stype` via `stype_info[stype].ltype`.
 *
 * meta
 *     Metadata associated with the column, if any, otherwise NULL. This is one
 *     of the *Meta structs defined in "types.h". The actual struct used
 *     depends on the `stype`.
 *
 * alloc_size
 *     Size (in bytes) of memory buffer `data`.
 *
 */
typedef struct Column {
    void   *data;        // 8
    void   *meta;        // 8
    size_t  alloc_size;  // 8
    int     refcount;    // 4
    MType   mtype;       // 1
    SType   stype;       // 1
    int16_t _padding;    // 2
} Column;



//==============================================================================

Column* make_column(SType stype, size_t nrows);
Column* column_extract(Column *col, RowMapping *rowmapping);
RowMapping* column_sort(Column *col, int64_t nrows);
void column_incref(Column *col);
void column_decref(Column *col);


#endif
