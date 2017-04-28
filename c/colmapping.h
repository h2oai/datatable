#ifndef dt_COLMAPPING_H
#define dt_COLMAPPING_H
#include "types.h"
#include "datatable.h"


typedef struct ColMapping {
    ssize_t length;
    ssize_t *indices;
    SType *stypes;
} ColMapping;


ColMapping* ColMapping_from_array(ssize_t *array, ssize_t len, DataTable *dt);
// ColMapping* ColMapping_alloc(ssize_t length);
void ColMapping_dealloc(ColMapping *colmapping);


#endif
