#include <stdlib.h>
#include "colmapping.h"


ColMapping*
ColMapping_from_array(int64_t *array, int64_t length, DataTable *dt)
{
    ColMapping *res = malloc(sizeof(ColMapping));
    if (res == NULL) return NULL;
    res->length = length;
    res->indices = array;
    res->stypes = malloc(sizeof(DataSType) * (size_t)length);
    if (res->stypes == NULL) goto fail;
    Column *columns = dt->columns;
    for (int64_t i = 0; i < length; i++) {
        res->stypes[i] = columns[array[i]].stype;
    }
    return res;

  fail:
    ColMapping_dealloc(res);
    return NULL;
}


/*
ColMapping* ColMapping_alloc(int64_t length)
{
    ColMapping *res = malloc(sizeof(ColMapping));
    if (res == NULL) return NULL;
    res->length = length;
    res->indices = malloc(sizeof(int64_t) * length);
    res->coltypes = malloc(sizeof(DataLType) * length);
    if (res->indices == NULL || res->coltypes == NULL) goto fail;
    return res;

  fail:
    ColMapping_dealloc(res);
    return NULL;
}
*/


void ColMapping_dealloc(ColMapping *colmapping)
{
    if (colmapping != NULL) {
        free(colmapping->indices);
        free(colmapping->stypes);
        free(colmapping);
    }
}
