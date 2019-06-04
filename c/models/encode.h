//------------------------------------------------------------------------------
// Copyright 2018 H2O.ai
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
#ifndef dt_MODELS_ENCODE_h
#define dt_MODELS_ENCODE_h
#include "datatable.h"
#include <unordered_map>  // std::unordered_map


namespace dt {
  struct EncodedLabels {
    dtptr dt_labels;
    dtptr dt_encoded;
  };


  EncodedLabels encode(Column*);
  template <SType>
  EncodedLabels encode_impl(Column*);
  template <SType stype>
  dtptr
  create_dt_labels_fw(const std::unordered_map<element_t<stype>, int32_t>&);
  template <typename T>
  dtptr create_dt_labels_str(const std::unordered_map<std::string, int32_t>&);
  EncodedLabels encode_bool(Column*);
}

#endif
