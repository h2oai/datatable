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
#include "reader.h"
#include <stdint.h>
#include "py_utils.h"


GenericReader::GenericReader(PyObject* pyrdr)
{
  int64_t nthreads64 = get_attr_int64(pyrdr, "nthreads");
  int nthreads = static_cast<int>(nthreads64);
  int maxth = omp_get_max_threads();
  if (nthreads > maxth) nthreads = maxth;
  if (nthreads <= 0) nthreads += maxth;
  if (nthreads <= 0) nthreads = 1;

}


bool GenericReader::read()
{
  return true;
}

