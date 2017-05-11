#ifndef dt_COLUMN_H
#define dt_COLUMN_H
#include <assert.h>    // static_assert
#include <inttypes.h>  // int*_t
#include <stddef.h>    // offsetof
#include "types.h"

typedef struct DataTable DataTable;
typedef struct RowMapping RowMapping;



//==============================================================================

typedef enum MType {
    MT_DATA  = 1,
    MT_MMAP  = 2,
    MT_VIEW  = 3,
} __attribute__ ((__packed__)) MType;



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
    void   *data;        // 8(4)
    MType   mtype;       // 1
    SType   stype;       // 1
    void   *meta;        // 8(4)
    size_t  alloc_size;  // 8(4)
} Column;



/**
 * A "view" column may only exist in a view datatable, and it replaces the
 * :struct:`Column` structure in the `columns` array. The `mtype` field
 * which is available on both :struct:`Column` and :struct:`ViewColumn` allows
 * us to distinguish between these 2 cases.
 *
 * The `srcindex` attribute gives the index of the column in the `source`
 * datatable to which the current column defers. The values from the "source"
 * column must be extracted according to the current datatable's `rowmapping`.
 *
 * srcindex
 *     The index of the column in the `source` datatable that the current
 *     column references.
 *
 * mtype
 *     Always MT_VIEW.
 *
 * stype
 *     Storage type of the referenced column.
 *
 */
typedef struct ViewColumn {
    size_t  srcindex;    // 8(4)
    MType   mtype;       // 1
    SType   stype;       // 1
} ViewColumn;



//==============================================================================

static_assert(sizeof(MType) == 1, "MType should be 1 byte in size");
static_assert(sizeof(SType) == 1, "SType should be 1 byte in size");
static_assert(offsetof(Column, mtype) == offsetof(ViewColumn, mtype),
              "mtype fields should be co-located to allow type punning");
static_assert(offsetof(Column, stype) == offsetof(ViewColumn, stype),
              "stype fields should be co-located to allow type punning");


/**
 * Extract data from Column `col` into a new Column object according to the
 * provided `rowmapping`. The new column will have mtype `MT_DATA`. This method
 * can be used to clone a column, or to convert a ViewColumn into a regular
 * Column.
 */
Column* column_extract(Column *col, RowMapping *rowmapping);

/**
 * Free all memory owned by the column, and then the column itself.
 */
void column_dealloc(Column *col);



#endif
