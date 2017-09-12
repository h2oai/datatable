#ifndef dt_MYOMP_H
#define dt_MYOMP_H

#ifdef DTNOOMP
    #define omp_get_max_threads() 1
    #define omp_get_num_threads() 1
    #define omp_get_thread_num() 0
#else
    #include <omp.h>
#endif

#endif
