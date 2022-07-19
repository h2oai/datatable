//------------------------------------------------------------------------------
// Copyright 2018-2022 H2O.ai
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
#ifndef dt_PARALLEL_STRING_UTILS_h
#define dt_PARALLEL_STRING_UTILS_h
#include <memory>             // std::unique_ptr
#include <vector>             // std::vector
#include "utils/function.h"   // dt::function
#include "rowindex.h"         // RowIndex
#include "wstringcol.h"       // writable_string_col
#include "stype.h"
namespace dt {


//------------------------------------------------------------------------------
// Iterate over range [0; n), producing a string column
//------------------------------------------------------------------------------

using string_buf = writable_string_col::buffer;

Column generate_string_column(dt::function<void(size_t, string_buf*)> fn,
                              size_t n,
                              Buffer&& offsets_buffer = Buffer(),
                              bool force_str64 = false,
                              bool force_single_threaded = false);




//------------------------------------------------------------------------------
// Map over a string column producing a new ostring column
//------------------------------------------------------------------------------

Column map_str2str(const Column& input_col,
                   dt::function<void(size_t, CString&, string_buf*)> fn);



}  // namespace dt
#endif
