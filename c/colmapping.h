#ifndef dt_COLMAPPING_H
#define dt_COLMAPPING_H
#include "coltype.h"
#include "datatable.h"


typedef struct ColMapping {
    int64_t length;
    int64_t *indices;
    ColType *coltypes;
} ColMapping;


ColMapping* ColMapping_from_array(int64_t *array, int64_t len, DataTable *dt);
// ColMapping* ColMapping_alloc(int64_t length);
void ColMapping_dealloc(ColMapping *colmapping);


#endif
