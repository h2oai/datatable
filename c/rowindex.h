#ifndef Dt_ROWS_H
#define Dt_ROWS_H
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
    DataTable *dt, int64_t start, int64_t count, int64_t step);
RowIndex* dt_select_row_indices(DataTable *dt, int64_t* data, int64_t len);
void dt_RowIndex_dealloc(RowIndex *ri);

#endif
