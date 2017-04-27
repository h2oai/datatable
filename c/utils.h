//==============================================================================
//  Collection of macros / helper functions
//==============================================================================
#ifndef dt_UTILS_H
#define dt_UTILS_H

inline int64_t min(int64_t a, int64_t b) { return a < b? a : b; }
inline int64_t max(int64_t a, int64_t b) { return a > b? a : b; }
inline float max_f4(float a, float b) { return a < b? b : a; }


/**
 * Helper method that attempts to compute object `x`, but executes "goto fail"
 * if the object turns out to be NULL.
 */
#define TRY(x) ({                                                              \
    void *y = x;                                                               \
    if (y == NULL) goto fail;                                                  \
    y;                                                                         \
})


#endif
