//==============================================================================
//  Collection of macros / helper functions
//==============================================================================
#ifndef dt_UTILS_H
#define dt_UTILS_H
#include <stddef.h>

int64_t min(int64_t a, int64_t b);
int64_t max(int64_t a, int64_t b);
float max_f4(float a, float b);


/**
 * Helper method that attempts to compute object `x`, but executes "goto fail"
 * if the object turns out to be NULL.
 */
#define TRY(x) ({                                                              \
    void *y = x;                                                               \
    if (y == NULL) goto fail;                                                  \
    y;                                                                         \
})



//==============================================================================
// Pointer macros
//==============================================================================

/**
 * Use this macro to malloc a memory region and assign it to a variable. For
 * example, to malloc an array of 1000 integers / a void* memory region of 1024
 * bytes:
 *
 *     int32_t *arr;
 *     dtmalloc(arr, int32_t, 1000);
 *     void *buf;
 *     dtmalloc_v(buf, 1024);
 *
 * The macro checks that the allocation was successful, and if not it executes
 * `return NULL`. Thus, it can only be used within a function that returns a
 * pointer.
 **/
#define dtmalloc(ptr, T, n) do {                                               \
    ptr = (T*) _dt_malloc((size_t)(n) * sizeof(T));                            \
    if (ptr == _NULL) return _NULL;                                            \
} while(0)

#define dtmalloc_v(ptr, n) do {                                                \
    ptr = _dt_malloc((size_t)(n));                                             \
    if (ptr == _NULL) return _NULL;                                            \
} while(0)


/**
 * This macro is a replacement for standard `realloc` in much the same way as
 * `dtmalloc` is a replacement for system `malloc`:
 *
 *     float *data = NULL;
 *     dtrealloc(data, float, 100);
 *
 * The macro checks that the allocation was successful, and if not it executes
 * `return NULL`. Thus, it can only be used within a function that returns a
 * pointer.
 */
#define dtrealloc(ptr, T, n) do {                                              \
    ptr = (T*) _dt_realloc(ptr, (size_t)(n) * sizeof(T));                      \
    if (ptr == _NULL) return _NULL;                                            \
} while(0)

#define dtrealloc_v(ptr, n) do {                                               \
    ptr = _dt_realloc(ptr, (size_t)(n));                                       \
    if (ptr == _NULL) return _NULL;                                            \
} while(0)


/**
 * Replacement for `calloc`.
 */
#define dtcalloc(ptr, T, n) do {                                               \
    ptr = (T*) _dt_calloc((size_t)(n), sizeof(T));                             \
    if (ptr == _NULL) return _NULL;                                            \
} while(0)



/**
 * Macro to free the memory previously allocated with `dtmalloc` / `dtrealloc`.
 * This frees the memory and sets the pointer to NULL, for safety.
 **/
#define dtfree(ptr) do {                                                       \
    _dt_free(ptr);                                                             \
    ptr = _NULL;                                                               \
} while(0)


/**
 * Macro for safe pointer arithmetic on `void*` pointers. Normally we'd want to
 * say simply `ptr + n`, but this is not in standard C, and produces a warning
 * on Clang compiler (and in C++ such operation is straight prohibited...).
 */
#define add_ptr(ptr, n)  ((void*)((unsigned char*)(ptr) + (n)))



//---- Private -------------------------
// The _dt_xxx functions are external and should be defined in py_utils.c
void* _dt_calloc(size_t n, size_t size);
void* _dt_malloc(size_t n);
void* _dt_realloc(void *ptr, size_t n);
void  _dt_free(void *ptr);
#ifdef __cplusplus
    #define _NULL nullptr
#else
    #define _NULL NULL
#endif


//==============================================================================
// Binary arithmetics
//==============================================================================

unsigned int nlz(uint32_t x);  // Number of leading zeros



//==============================================================================
// Other
//==============================================================================

/**
 * Mark a function parameter as deliberately unused.
 **/
#define UNUSED(x)  __attribute__((unused)) x



#endif
