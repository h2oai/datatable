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

// Some macro hackery based on
// https://github.com/pfultz2/Cloak/wiki/C-Preprocessor-tricks,-tips,-and-idioms
#define zPRIMITIVE_CAT(a, ...) a ## __VA_ARGS__
#define zCAT(a, ...) zPRIMITIVE_CAT(a, __VA_ARGS__)
#define zCOMPL(b) zPRIMITIVE_CAT(zCOMPL_, b)
#define zCOMPL_0 1
#define zCOMPL_1 0
#define zEAT(...)
#define zBITAND(x) zPRIMITIVE_CAT(zBITAND_, x)
#define zBITAND_0(y) 0
#define zBITAND_1(y) y
#define zIIF_0(t, ...) __VA_ARGS__
#define zIIF_1(t, ...) t
#define zCHECK_N(x, n, ...) n
#define zCHECK(...) zCHECK_N(__VA_ARGS__, 0,)
#define zPROBE(x) x, 1,
#define zIS_PAREN(x) zCHECK(zIS_PAREN_PROBE x)
#define zIS_PAREN_PROBE(...) zPROBE(~)
#define zIS_COMPARABLE(x) zIS_PAREN(zCAT(zCOMPARE_, x) (()))
#define zPRIMITIVE_COMPARE(x, y) zIS_PAREN (zCOMPARE_ ## x (zCOMPARE_ ## y)(()))
#define zNOT_EQUAL(x, y) zIIF(zBITAND(zIS_COMPARABLE(x))(zIS_COMPARABLE(y))) \
                         (zPRIMITIVE_COMPARE, 1 zEAT)(x, y)
#define zIIF(c) zPRIMITIVE_CAT(zIIF_, c)
#define zEQUAL(x, y) zCOMPL(zNOT_EQUAL(x, y))
// All for the benefit of this tiny macro, which expands to `sizeof(T)` in all
// cases except when `T` is `void`, in which case it expands into `1`.
#define SIZEOF(T) zIIF(zEQUAL(T, void))(1, sizeof(T))


/**
 * Use this macro to malloc a memory region and assign it to a variable. For
 * example, to malloc an array of 1000 integers:
 *
 *     int32_t *arr;
 *     dtmalloc(arr, int32_t, 1000);
 *
 * This will also work correctly for allocating a `void*` pointer:
 *
 *     void *buf;
 *     dtmalloc(buf, void, 1024 * 1024);
 *
 * The macro checks that the allocation was successful, and if not it executes
 * `return NULL`. Thus, it can only be used within a function that returns a
 * pointer. The `_g` version of this macro instead executes `goto fail`, where
 * any special cleanup may be performed.
 **/
#define dtmalloc(ptr, T, n) do {                                               \
    ptr = (T*) _dt_malloc((size_t)(n) * SIZEOF(T));                            \
    if (ptr == zNULL && n) return zNULL;                                       \
} while(0)

#define dtmalloc_g(ptr, T, n) do {                                             \
    ptr = (T*) _dt_malloc((size_t)(n) * SIZEOF(T));                            \
    if (ptr == zNULL && n) goto fail;                                          \
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
 * pointer. The `_g` version of this macro instead executes `goto fail`, where
 * any special cleanup may be performed.
 */
#define dtrealloc(ptr, T, n) do {                                              \
    ptr = (T*) _dt_realloc(ptr, (size_t)(n) * SIZEOF(T));                      \
    if (ptr == zNULL && n) return zNULL;                                       \
} while(0)

#define dtrealloc_g(ptr, T, n) do {                                            \
    ptr = (T*) _dt_realloc(ptr, (size_t)(n) * SIZEOF(T));                      \
    if (ptr == zNULL && n) goto fail;                                          \
} while(0)



/**
 * Replacement for `calloc`.
 */
#define dtcalloc(ptr, T, n) do {                                               \
    ptr = (T*) _dt_calloc((size_t)(n), SIZEOF(T));                             \
    if (ptr == zNULL && n) return zNULL;                                       \
} while(0)

#define dtcalloc_g(ptr, T, n) do {                                             \
    ptr = (T*) _dt_calloc((size_t)(n), SIZEOF(T));                             \
    if (ptr == zNULL && n) goto fail;                                          \
} while(0)



/**
 * Macro to free the memory previously allocated with `dtmalloc` / `dtrealloc`.
 * This frees the memory and sets the pointer to NULL, for safety.
 **/
#define dtfree(ptr) do {                                                       \
    _dt_free(ptr);                                                             \
    ptr = zNULL;                                                               \
} while(0)



/**
 * Macro for safe pointer arithmetic on `void*` pointers. Normally we'd want to
 * say simply `ptr + n`, but this is not in standard C, and produces a warning
 * on Clang compiler (and in C++ such operation is straight prohibited...).
 */
#define add_ptr(ptr, n)       ((void*)((char*)(ptr) + (n)))
#define add_constptr(ptr, n)  ((const void*)((const char*)(ptr) + (n)))


//---- Private -------------------------
// The _dt_xxx functions are external and should be defined in py_utils.c
void* _dt_calloc(size_t n, size_t size);
void* _dt_malloc(size_t n);
void* _dt_realloc(void *ptr, size_t n);
void  _dt_free(void *ptr);
#ifdef __cplusplus
    #define zNULL nullptr
#else
    #define zNULL NULL
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
