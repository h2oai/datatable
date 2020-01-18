//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#ifndef dt_UTILS_ALLLOC_h
#define dt_UTILS_ALLLOC_h
#include <cstdlib>
#include <cstdint>

namespace dt
{
void free(void*);
void* _realloc(void*, size_t);



template <typename T> inline T* malloc(size_t n) {
  return static_cast<T*>(_realloc(nullptr, n));
}

template <typename T> inline T* amalloc(size_t n) {
  return static_cast<T*>(_realloc(nullptr, n * sizeof(T)));
}

template <typename T> inline T* amalloc(int64_t n) {
  return static_cast<T*>(_realloc(nullptr, static_cast<size_t>(n) * sizeof(T)));
}


template <typename T> inline T* realloc(T* ptr, size_t n) {
  return static_cast<T*>(_realloc(ptr, n));
}

template <typename T> inline T* arealloc(T* ptr, size_t n) {
  return static_cast<T*>(_realloc(ptr, n * sizeof(T)));
}

template <typename T> inline T* arealloc(T* ptr, int64_t n) {
  return static_cast<T*>(_realloc(ptr, static_cast<size_t>(n) * sizeof(T)));
}

}; // namespace dt
#endif
