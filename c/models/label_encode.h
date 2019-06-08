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

  void label_encode(const Column*, dtptr&, dtptr&, bool is_binomial = false);

  void label_encode_bool(const Column*, dtptr&, dtptr&);

  template <SType, SType>
  void label_encode_fw(const Column*, dtptr&, dtptr&);
  template <typename, SType>
  void label_encode_str(const Column*, dtptr&, dtptr&);

  template <SType stype_from, SType stype_to>
  dtptr create_dt_labels_fw(const std::unordered_map<element_t<stype_from>, element_t<stype_to>>&);
  template <typename, SType stype_to>
  dtptr create_dt_labels_str(const std::unordered_map<std::string, element_t<stype_to>>&);

}

#endif
