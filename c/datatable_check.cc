#include "datatable_check.h"


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


__attribute__ ((format (printf, 2, 0)))
void push_error(std::vector<char> *out, const char* format, ...) {
  if (out == nullptr) return;
  static char buf[1001];
  va_list args;
  va_start(args, format);
  int ret = vsnprintf(buf, 1001, format, args);
  if (ret < 0) return;
  va_end(args);
  size_t add_len = strlen(buf) + 1; // +1 because of '\0'
  size_t len = out->size();
  if (len != 0) --len;
  out->resize(len + add_len);
  memcpy(out->data() + len, buf, add_len);
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

