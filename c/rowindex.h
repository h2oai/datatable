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



RowIndex* RowIndex_from_slice(int64_t start, int64_t count, int64_t step);
RowIndex* RowIndex_from_array(int64_t* array, int64_t length);
RowIndex* RowIndex_merge(RowIndex *risrc, RowIndex *rinew);
void RowIndex_dealloc(RowIndex *rowindex);


#endif
