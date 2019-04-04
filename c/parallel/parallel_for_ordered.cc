//------------------------------------------------------------------------------
// Copyright 2019 H2O.ai
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//------------------------------------------------------------------------------
#include "parallel/api.h"
#include "utils/assert.h"
#include "utils/function.h"
namespace dt {


//------------------------------------------------------------------------------
// parallel_for_ordered
//------------------------------------------------------------------------------

void parallel_for_ordered(size_t nrows,
                          function<void(size_t)> pre_ordered,
                          function<void(size_t)> ordered,
                          function<void(size_t)> post_ordered)
{
  thread_pool* thpool = thread_pool::get_instance();
  size_t ith = dt::get_thread_num();
}



}  // namespace dt
