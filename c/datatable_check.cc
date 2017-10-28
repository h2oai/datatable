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
#include "datatable_check.h"
// #include <limits>    // std::numeric_limits
#include "utils.h"


/**
 * Return the size of the array `ptr`, or 0 if the platform doesn't allow
 * such functionality.
 */
size_t array_size(void *ptr, size_t elemsize) {
    #ifdef MALLOC_SIZE_UNAVAILABLE
    return 0;
    #endif
    return ptr == NULL? 0 : malloc_size(ptr) / elemsize;
}


char*
repr_utf8(const unsigned char* ptr0, const unsigned char* ptr1) {
    static char buf[101];
    int i = 0;
    for (const unsigned char *ptr = ptr0; ptr < ptr1; ptr++) {
        if (*ptr >= 0x20 && *ptr < 0x7F)
            buf[i++] = (char) *ptr;
        else {
            int8_t d0 = (*ptr) & 0xF;
            int8_t d1 = (*ptr) >> 4;
            buf[i++] = '\\';
            buf[i++] = 'x';
            buf[i++] = d1 <= 9? ('0' + d1) : ('A' + d1 - 10);
            buf[i++] = d0 <= 9? ('0' + d0) : ('A' + d0 - 10);
        }
        if (i >= 95) break;
    }
    buf[i] = '\0';
    return buf;
}

const EndOfError IntegrityCheckContext::eoe;


IntegrityCheckContext::IntegrityCheckContext(int max) {
  if (max < 0) max = 10000;
  max_errors = max;
  num_errors = 0;
}


IntegrityCheckContext& IntegrityCheckContext::operator<<(const std::string& s) {
  if (num_errors < max_errors)
    error_stream << s;
  return *this;
}

IntegrityCheckContext& IntegrityCheckContext::operator<<(const char* s) {
  if (num_errors < max_errors)
    error_stream << s;
  return *this;
}

IntegrityCheckContext& IntegrityCheckContext::operator<<(const void* s) {
  if (num_errors < max_errors)
    error_stream << s;
  return *this;
}

IntegrityCheckContext& IntegrityCheckContext::operator<<(int64_t num) {
  if (num_errors < max_errors)
    error_stream << num;
  return *this;
}

IntegrityCheckContext& IntegrityCheckContext::operator<<(int32_t num) {
  if (num_errors < max_errors)
    error_stream << num;
  return *this;
}

IntegrityCheckContext& IntegrityCheckContext::operator<<(int8_t num) {
  if (num_errors < max_errors)
    error_stream << num;
  return *this;
}

IntegrityCheckContext& IntegrityCheckContext::operator<<(size_t num) {
  if (num_errors < max_errors)
    error_stream << num;
  return *this;
}

void IntegrityCheckContext::operator<<(const EndOfError&) {
  if (num_errors < max_errors)
    error_stream << "\n";
  num_errors++;
}
