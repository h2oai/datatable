//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#ifndef dt_SORT_h
#define dt_SORT_h
#include <stdint.h>

void* sort_i1(int8_t *x, int32_t n, int32_t **o);
void* sort_i4(int32_t *x, int32_t n, int32_t **o);
void init_sort_functions(void);


template <typename T, typename I>
I* insert_sort_fw(const T* x, I* o, I* oo, int n);


extern template int32_t* insert_sort_fw(const int8_t*,  int32_t*, int32_t*, int);
extern template int32_t* insert_sort_fw(const int16_t*, int32_t*, int32_t*, int);
extern template int32_t* insert_sort_fw(const int32_t*, int32_t*, int32_t*, int);
extern template int32_t* insert_sort_fw(const int64_t*, int32_t*, int32_t*, int);
extern template int32_t* insert_sort_fw(const uint8_t*, int32_t*, int32_t*, int);
extern template int32_t* insert_sort_fw(const uint16_t*, int32_t*, int32_t*, int);
extern template int32_t* insert_sort_fw(const uint32_t*, int32_t*, int32_t*, int);
extern template int32_t* insert_sort_fw(const uint64_t*, int32_t*, int32_t*, int);


#endif
