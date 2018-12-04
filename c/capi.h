//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#ifndef dt_CAPI_H
#define dt_CAPI_H
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif


void* datatable_get_column_data(void* dt, size_t column);
void  datatable_unpack_slicerowindex(void* dt, size_t* start, size_t* step);
void  datatable_unpack_arrayrowindex(void* dt, void** indices);

#ifdef __cplusplus
}
#endif

#endif
