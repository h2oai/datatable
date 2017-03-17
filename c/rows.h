#ifndef Dt_ROWS_H
#define Dt_ROWS_H
#include <Python.h>
#include "datatable.h"

typedef enum RowIndexType {
    RI_ARRAY,
    RI_SLICE,
} RowIndexType;


typedef struct RowIndex {
    RowIndexType type;
    int64_t length;
    union {
        int64_t* indices;
        struct { int64_t start, step; } slice;
    };
} RowIndex;


RowIndex* dt_select_row_slice(
    dt_DatatableObject *dt, int64_t start, int64_t count, int64_t step);
RowIndex* dt_select_row_indices(
    dt_DatatableObject *dt, int64_t* data, int64_t len);
void dt_rowindex_dealloc(RowIndex *ri);

#endif
