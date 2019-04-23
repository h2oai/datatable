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
#ifndef dt_PROGRESS_COMMON_h
#define dt_PROGRESS_COMMON_h
#include <cstddef>
namespace dt {
namespace progress {
using std::size_t;


enum class Status : int8_t {
  RUNNING = 0,
  FINISHED = 1,
  ERROR = 2,
  CANCELLED = 3,
};


// forward-declare
class progress_bar;
class progress_manager;
class work;


// called from core.initialize_options() (see datatablemodule.cc)
void init_options();


}} // namespace dt::progress
#endif
