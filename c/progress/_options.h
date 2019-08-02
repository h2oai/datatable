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
// This file is named with _ only because it would otherwise conflict with
// "options.h" from the parent directory.
#ifndef dt_PROGRESS_OPTIONS_h
#define dt_PROGRESS_OPTIONS_h
#include "python/python.h"
namespace dt {
namespace progress {


// forward-declare
class progress_bar;
class progress_manager;
class work;


// options
extern double updates_per_second;
extern double min_duration;
extern PyObject* progress_fn;
extern bool enabled;
extern bool clear_on_success;



// called from core.initialize_options() (see datatablemodule.cc)
void init_options();


}} // namespace dt::progress
#endif
