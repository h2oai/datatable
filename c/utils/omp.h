//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#ifndef dt_UTILS_OMP_H
#define dt_UTILS_OMP_H

#ifdef DTNOOMP
  #define omp_get_max_threads() 1
  #define omp_get_num_threads() 1
  #define omp_get_thread_num() 0
#else
  #include <omp.h>
#endif


#endif
