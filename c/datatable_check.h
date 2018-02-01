//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
// See `meth_check()` in "py_datatable.c"
//
#ifndef dt_DATATABLE_CHECK_H
#define dt_DATATABLE_CHECK_H
#include <string>
#include <sstream>
// #include <vector>

// "Sentinent" object to
class EndOfError {};


class IntegrityCheckContext
{
  static const EndOfError eoe;
  std::ostringstream error_stream;
  int num_errors;
  int max_errors;

public:
  IntegrityCheckContext(int max);
  bool has_errors(int n = 0) const { return num_errors > n; }
  int n_errors() const { return num_errors; }
  const EndOfError& end() const { return eoe; }
  const std::ostringstream& errors() const { return error_stream; }

  IntegrityCheckContext& operator<<(const std::string&);
  IntegrityCheckContext& operator<<(const char*);
  IntegrityCheckContext& operator<<(const void*);
  IntegrityCheckContext& operator<<(int64_t);
  IntegrityCheckContext& operator<<(int32_t);
  IntegrityCheckContext& operator<<(int8_t);
  IntegrityCheckContext& operator<<(size_t);
  void operator<<(const EndOfError&);
};



#ifdef __APPLE__
  #include <malloc/malloc.h>  // size_t malloc_size(const void *)
#elif defined _WIN32
  #include <malloc.h>  // size_t _msize(void *)
  #define malloc_size  _msize
#elif defined __linux__
  #include <malloc.h>  // size_t malloc_usable_size(void *) __THROW
  #define malloc_size  malloc_usable_size
#else
  #define malloc_size(p)  0
  #define MALLOC_SIZE_UNAVAILABLE
#endif

char*
repr_utf8(const unsigned char* ptr0, const unsigned char* ptr1);


size_t array_size(void *ptr, size_t elemsize);


#endif
