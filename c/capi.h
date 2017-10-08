#ifndef dt_CAPI_H
#define dt_CAPI_H
#include "stdint.h"

#ifdef __cplusplus
extern "C" {
#endif


void* datatable_get_column_data(void* dt, int column);
void  datatable_unpack_slicerowindex(void *dt, int64_t *start, int64_t *step);
void  datatable_unpack_arrayrowindex(void *dt, void **indices);

#ifdef __cplusplus
}
#endif

#endif
