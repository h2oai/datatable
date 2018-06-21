//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
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
  #else
    return ptr == nullptr? 0 : malloc_size(ptr) / elemsize;
  #endif
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

IntegrityCheckContext& IntegrityCheckContext::operator<<(uint32_t num) {
  if (num_errors < max_errors)
    error_stream << num;
  return *this;
}

IntegrityCheckContext& IntegrityCheckContext::operator<<(uint64_t num) {
  if (num_errors < max_errors)
    error_stream << num;
  return *this;
}

void IntegrityCheckContext::operator<<(const EndOfError&) {
  if (num_errors < max_errors)
    error_stream << "\n";
  num_errors++;
}
