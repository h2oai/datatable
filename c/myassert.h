#ifdef NDEBUG
    // Macro NDEBUG, if present, disables all assert statements. Unfortunately,
    // Python turns on this macro by default, so we need to turn it off
    // explicitly.
    #ifdef NONDEBUG
        #undef NDEBUG
    #endif
#endif
#include <assert.h>  // assert, static_assert

#ifndef static_assert
    // Some system libraries fail to define this macro :(
    // In this case we just disable these checks.
    #define static_assert(cond)
#endif
