#ifndef dt_COLUMN_H
#define dt_COLUMN_H
#include <inttypes.h>  // int*_t
#include <stddef.h>    // offsetof
#include "memorybuf.h"
#include "types.h"
#include "stats.h"

typedef struct DataTable DataTable;
class RowIndex;
class Stats;

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
class Column {
    MemoryBuffer *mbuf;

public:  // TODO: convert these into private
    void   *meta;        // 8
    int64_t nrows;       // 8
    Stats*  stats;       // 8
    int     refcount;    // 4
    SType   stype;       // 1

private:
    __attribute__((unused)) char _padding[3];

    Column(size_t nrows_, SType stype_); // helper for other constructors
    static size_t allocsize0(SType, size_t n);

public:
    Column(SType, size_t); // Data Column
    Column(SType, size_t, const char*); // MMap Column
    Column(SType, size_t, void*, void*, size_t); // XBuf Column
    Column(const char*, SType, size_t, const char*); // Load from disk
    explicit Column(const Column&);

    void* data() const;
    size_t alloc_size() const;
    PyObject* mbuf_repr() const;

    Column* cast(SType);
    Column* rbind(Column**);
    Column* extract(RowIndex* = NULL);
    Column* realloc_and_fill(int64_t);
    Column* save_to_disk(const char*);
    size_t i4s_datasize();
    size_t i8s_datasize();
    size_t get_allocsize();
    Column* incref();
    void decref();

    static RowIndex* sort(Column*, RowIndex*);
    static size_t i4s_padding(size_t datasize);
    static size_t i8s_padding(size_t datasize);

private:
    Column* rbind_fw(Column**, int64_t, int);  // helper for rbind
    Column* rbind_str32(Column**, int64_t, int);
    ~Column() {}

    // FIXME
    friend Column* try_to_resolve_object_column(Column* col);
    friend Column* column_from_list(PyObject *list);
    friend Column* realloc_column(Column *col, SType stype, size_t nrows, int j);
    friend void setFinalNrow(size_t nrows);
};



//==============================================================================
typedef Column* (*castfn_ptr)(Column*, Column*);

Column* column_from_list(PyObject*);
void init_column_cast_functions(void);
// Implemented in py_column_cast.c
void init_column_cast_functions2(castfn_ptr hardcasts[][DT_STYPES_COUNT]);

#endif
