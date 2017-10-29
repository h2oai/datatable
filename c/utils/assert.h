//------------------------------------------------------------------------------
//  Copyright 2017 H2O.ai
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.
//------------------------------------------------------------------------------
#ifndef dt_UTILS_ASSERT_H
#define dt_UTILS_ASSERT_H

#ifdef NDEBUG
  // Macro NDEBUG, if present, disables all assert statements. Unfortunately,
  // Python turns on this macro by default, so we need to turn it off
  // explicitly.
  #ifdef NONDEBUG
    #undef NDEBUG
  #endif
#endif
#include <assert.h>  // assert, dt_static_assert

#ifdef static_assert
  #define dt_static_assert static_assert
#else
  // Some system libraries fail to define this macro :( At least 3 people have
  // reported having this problem on the first day of testing, so it's not
  // that uncommon.
  // In this case we just disable the checks.
  #define dt_static_assert(x, y)
#endif


#endif
