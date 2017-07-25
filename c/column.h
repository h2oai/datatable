#ifndef dt_COLUMN_H
#define dt_COLUMN_H
#include <inttypes.h>  // int*_t
#include <stddef.h>    // offsetof
#include "types.h"

typedef struct DataTable DataTable;
typedef struct RowIndex RowIndex;

//==============================================================================

/**
 * "Memory" type of the column -- i.e. where the data is actually stored.
 * Columns with different `MType`s are generally interchangeable, except that
 * they may require different strategies for allocating/ reallocating/ freeing
 * their data buffers.
 *
 * MT_DATA
 *     The data is stored in RAM. This is the most common mtype for a column.
 *     When this column is deleted, its memory buffer will be freed.
 *
 * MT_MMAP
 *     The data is stored on disk, but memory-mapped into RAM. Such column is
 *     read-only. When the column is deleted its memory buffer is unmapped, but
 *     the file remains on disk.
 *
 * MT_TEMP
 *     Similar to MT_MMAP, but the data is stored in a temporary file. When the
 *     column is removed, the underlying temporary file is deleted as well.
 *     Uses field `filename`. Uses field `filename`.
 *
 * MT_XBUF
 *     The data is stored in external buffer, obtained via PyBuffers protocol.
 *     Such data is read-only, and when the column is deleted, the buffer has
 *     to be released. Uses field `pybuf`.
 */
typedef enum MType {
    MT_DATA = 1,
    MT_MMAP = 2,
    MT_TEMP = 3,
    MT_XBUF = 4,
} __attribute__ ((__packed__)) MType;

#define MT_COUNT (MT_XBUF + 1)


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
 * Parameters
 * ----------
 * data
 *     Raw data buffer in NFF format, depending on the column's `stype` (see
 *     specification in "types.h"). This may be NULL if column has 0 rows.
 *
 * mtype
 *     "Memory" type of the column -- i.e. where the data is actually stored.
 *     Possible values are MT_DATA (column is stored in RAM), MT_MMAP (memory-
 *     -mapped from disk), and MT_XBUF (acquired from an external buffer).
 *
 * stype
 *     Type of data within the column (the enum is defined in types.h). This is
 *     the "storage" type, i.e. how the data is actually stored in memory. The
 *     logical type can be derived from `stype` via `stype_info[stype].ltype`.
 *
 * nrows
 *     Number of elements in this column. For fixed-size stypes, this should be
 *     equal to `alloc_size / stype_info[stype].elemsize`.
 *
 * meta
 *     Metadata associated with the column, if any, otherwise NULL. This is one
 *     of the *Meta structs defined in "types.h". The actual struct used
 *     depends on the `stype`.
 *
 * alloc_size
 *     Size (in bytes) of memory buffer `data`. For columns of type MT_MMAP this
 *     size is equal to the file size.
 *
 * refcount
 *     Number of references to this Column object. It is a common for a single
 *     Column to be reused across multiple DataTables (for example when a new
 *     DataTable is created as a view onto an existing one). In such case the
 *     refcount of the Column should be increased. Any modification to a Column
 *     with refcount > 1 is not allowed -- a copy should be made first. When a
 *     DataTable is deleted, it decreases refcounts of all its constituent
 *     Columns, and once a Column's refcount becomes 0 it can be safely deleted.
 */
typedef struct Column {
    void   *data;        // 8
    void   *meta;        // 8
    int64_t nrows;       // 8
    size_t  alloc_size;  // 8
    union {              // 8
        char *filename;
        void *pybuf;
    };
    int     refcount;    // 4
    MType   mtype;       // 1
    SType   stype;       // 1
    int16_t _padding;    // 2
} Column;



//==============================================================================
typedef Column* (*castfn_ptr)(Column*, Column*);

Column* make_data_column(SType stype, size_t nrows);
Column* make_mmap_column(SType stype, size_t nrows, const char *filename);
Column* column_copy(Column *self);
Column* column_cast(Column *self, SType stype);
Column* column_rbind(Column *self, Column **cols);
Column* column_extract(Column *self, RowIndex *rowindex);
Column* column_realloc_and_fill(Column *self, int64_t nrows);
Column* column_save_to_disk(Column *self, const char *filename);
Column* column_load_from_disk(const char *filename, SType stype, int64_t nrows,
                              const char *metastr);
Column* column_from_buffer(SType stype, int64_t nrows, void* pybuffer,
                           void* buf, size_t alloc_size);
RowIndex* column_sort(Column *self, int64_t nrows);
size_t column_i4s_padding(size_t datasize);
size_t column_i8s_padding(size_t datasize);
size_t column_i4s_datasize(Column *self);
size_t column_i8s_datasize(Column *self);
Column* column_incref(Column *self);
void column_decref(Column *self);

void init_column_cast_functions(void);
// Implemented in py_column_cast.c
void init_column_cast_functions2(castfn_ptr hardcasts[][DT_STYPES_COUNT]);

#endif
