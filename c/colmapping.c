#include <stdlib.h>
#include "colmapping.h"


ColMapping*
ColMapping_from_array(ssize_t *array, ssize_t length, DataTable *dt)
{
    ColMapping *res = malloc(sizeof(ColMapping));
    if (res == NULL) return NULL;
    res->length = length;
    res->indices = array;
    res->stypes = malloc(sizeof(SType) * (size_t)length);
    if (res->stypes == NULL) goto fail;
    Column **columns = dt->columns;
    for (ssize_t i = 0; i < length; i++) {
        res->stypes[i] = columns[array[i]]->stype;
    }
    return res;

  fail:
    ColMapping_dealloc(res);
    return NULL;
}


/*
ColMapping* ColMapping_alloc(ssize_t length)
{
    ColMapping *res = malloc(sizeof(ColMapping));
    if (res == NULL) return NULL;
    res->length = length;
    res->indices = malloc(sizeof(ssize_t) * length);
    res->coltypes = malloc(sizeof(LType) * length);
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
