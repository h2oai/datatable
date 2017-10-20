//------------------------------------------------------------------------------
//  Copyright 2017 H2O.ai
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.
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

__attribute__ ((format (printf, 2, 0)))
void push_error(std::vector<char> *out, const char* format, ...);
size_t array_size(void *ptr, size_t elemsize);

#define ERR(...) do {                                                           \
  if (nerrors++ <= max_errors)                                                  \
    push_error(errors, __VA_ARGS__);                                            \
  } while(0)


#endif
