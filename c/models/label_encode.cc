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
#include "models/label_encode.h"


namespace dt {


/**
 *  Encode column values with integers. If `is_binomial == true`,
 *  the function expects two classes at maximum and will emit an error,
 *  if there is more.
 */
void label_encode(const Column* col, dtptr& dt_labels, dtptr& dt_encoded,
                  bool is_binomial /* = false =*/) {
  xassert(dt_labels == nullptr);
  xassert(dt_encoded == nullptr);

  SType stype = col->stype();
  if (is_binomial) {
    switch (stype) {
      case SType::BOOL:    label_encode_bool(col, dt_labels, dt_encoded); break;
      case SType::INT8:    label_encode_fw<SType::INT8, SType::BOOL>(col, dt_labels, dt_encoded); break;
      case SType::INT16:   label_encode_fw<SType::INT16, SType::BOOL>(col, dt_labels, dt_encoded); break;
      case SType::INT32:   label_encode_fw<SType::INT32, SType::BOOL>(col, dt_labels, dt_encoded); break;
      case SType::INT64:   label_encode_fw<SType::INT64, SType::BOOL>(col, dt_labels, dt_encoded); break;
      case SType::FLOAT32: label_encode_fw<SType::FLOAT32, SType::BOOL>(col, dt_labels, dt_encoded); break;
      case SType::FLOAT64: label_encode_fw<SType::FLOAT64, SType::BOOL>(col, dt_labels, dt_encoded); break;
      case SType::STR32:   label_encode_str<uint32_t, SType::BOOL>(col, dt_labels, dt_encoded); break;
      case SType::STR64:   label_encode_str<uint64_t, SType::BOOL>(col, dt_labels, dt_encoded); break;

      default:             throw TypeError() << "Column type `" << stype << "` is not supported";
    }
  } else {
    switch (stype) {
      case SType::BOOL:    label_encode_bool(col, dt_labels, dt_encoded); break;
      case SType::INT8:    label_encode_fw<SType::INT8, SType::INT32>(col, dt_labels, dt_encoded); break;
      case SType::INT16:   label_encode_fw<SType::INT16, SType::INT32>(col, dt_labels, dt_encoded); break;
      case SType::INT32:   label_encode_fw<SType::INT32, SType::INT32>(col, dt_labels, dt_encoded); break;
      case SType::INT64:   label_encode_fw<SType::INT64, SType::INT32>(col, dt_labels, dt_encoded); break;
      case SType::FLOAT32: label_encode_fw<SType::FLOAT32, SType::INT32>(col, dt_labels, dt_encoded); break;
      case SType::FLOAT64: label_encode_fw<SType::FLOAT64, SType::INT32>(col, dt_labels, dt_encoded); break;
      case SType::STR32:   label_encode_str<uint32_t, SType::INT32>(col, dt_labels, dt_encoded); break;
      case SType::STR64:   label_encode_str<uint64_t, SType::INT32>(col, dt_labels, dt_encoded); break;

      default:             throw TypeError() << "Column type `" << stype << "` is not supported";
    }
  }

  // Set key to the labels column for later joining with the new labels.
  if (dt_labels != nullptr) {
    intvec keys{ 0 };
    dt_labels->set_key(keys);
  }
}


/**
 *  For boolean columns we do NA check and create boolean labels, i.e.
 *  false/true. No encoding in this case is necessary, so `dt_encoded`
 *  just uses a shallow copy of `col`.
 */
void label_encode_bool(const Column* col, dtptr& dt_labels, dtptr& dt_encoded) {
  auto data = static_cast<const int8_t*>(col->data());

  // If we only got NA's, return
  bool nas_only = true;
  for (size_t i = 0; i < col->nrows; ++i) {
    if (!ISNA<int8_t>(data[i])) {
      nas_only = false;
      break;
    }
  }
  if (nas_only) return;

  // Set up boolean labels and their corresponding ids.
  auto ids_col = new BoolColumn(2);
  auto labels_col = new BoolColumn(2);
  auto ids_data = ids_col->elements_w();
  auto labels_data = labels_col->elements_w();
  ids_data[0] = 0;
  ids_data[1] = 1;
  labels_data[0] = 0;
  labels_data[1] = 1;

  dt_labels = dtptr(new DataTable({labels_col, ids_col}, {"label", "id"}));
  dt_encoded = dtptr(new DataTable({col->shallowcopy()}));
}


} // namespace dt
