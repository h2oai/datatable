//==============================================================================
//  Collection of macros / helper functions
//==============================================================================
#ifndef dt_UTILS_H
#define dt_UTILS_H

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


/**
 * Use this macro to malloc a memory region and assign it to a variable. For
 * example, to malloc an array of 1000 integers:
 *
 *     int32_t *arr;
 *     dtmalloc(arr, int32_t, 1000);
 *
 * The macro checks that the allocation was successful, and if not it executes
 * `return NULL`.
 **/
#ifdef __cplusplus
    #define dtmalloc(ptr, T, n) do {                                           \
        ptr = (T*) malloc((size_t)(n) * sizeof(T));                            \
        if (ptr == nullptr) return nullptr;                                    \
    } while(0)
    #define dtmalloc_v(ptr, n) do {                                            \
        ptr = malloc((size_t)(n));                                             \
        if (ptr == nullptr) return nullptr;                                    \
    } while(0)
    #define dtrealloc(ptr, T, n) do {                                          \
        ptr = (T*) realloc(ptr, (size_t)(n) * sizeof(T));                      \
        if (ptr == nullptr) return nullptr;                                    \
    } while(0)
    #define dtrealloc_v(ptr, n) do {                                           \
        ptr = realloc(ptr, (size_t)(n));                                       \
        if (ptr == nullptr) return nullptr;                                    \
    } while(0)
#else
    #define dtmalloc(ptr, T, n) do {                                           \
        ptr = malloc((size_t)(n) * sizeof(T));                                 \
        if (ptr == NULL) return NULL;                                          \
    } while(0)
    #define dtmalloc_v(ptr, n) do {                                            \
        ptr = malloc((size_t)(n));                                             \
        if (ptr == NULL) return NULL;                                          \
    } while(0)
    #define dtrealloc(ptr, T, n) do {                                          \
        ptr = realloc(ptr, (size_t)(n) * sizeof(T));                           \
        if (ptr == NULL) return NULL;                                          \
    } while(0)
    #define dtrealloc_v(ptr, n) do {                                           \
        ptr = realloc(ptr, (size_t)(n));                                       \
        if (ptr == NULL) return NULL;                                          \
    } while(0)
#endif


/**
 * Macro to free the memory previously allocated with `dtmalloc`.
 * (Currently it's same as system `free`, however that may change in the future).
 **/
#define dtfree  free


/**
 * Macro for safe pointer arithmetic on `void*` pointers. Normally we'd want to
 * say simply `ptr + n`, but this is not in standard C, and produces a warning
 * on Clang compiler (and in C++ such operation is straight prohibited...).
 */
#define add_ptr(ptr, n)  ((void*)((unsigned char*)(ptr) + (n)))



/**
 * Mark a function parameter as deliberately unused.
 **/
#define UNUSED(x)  __attribute__((unused)) x



#endif
