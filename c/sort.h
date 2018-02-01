//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#ifndef dt_SORT_H
#define dt_SORT_H
#include <stddef.h>
#include <stdint.h>

void* sort_i1(int8_t *x, int32_t n, int32_t **o);
void* sort_i4(int32_t *x, int32_t n, int32_t **o);
void init_sort_functions(void);


#endif
