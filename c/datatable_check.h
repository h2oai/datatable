#ifndef C_DATATABLE_CHECK_H_
#define C_DATATABLE_CHECK_H_

#include <vector>

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

#endif /* C_DATATABLE_CHECK_H_ */
