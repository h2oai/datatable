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


static void label_encode_bool(const Column&, dtptr&, dtptr&);


/**
 *  Encode column values with integers. If `is_binomial == true`,
 *  the function expects two classes at maximum and will emit an error,
 *  if there is more.
 */
void label_encode(const Column& col, dtptr& dt_labels, dtptr& dt_encoded,
                  bool is_binomial /* = false =*/) {
  xassert(dt_labels == nullptr);
  xassert(dt_encoded == nullptr);

  SType stype = col.stype();
  switch (stype) {
    case SType::BOOL:    label_encode_bool(col, dt_labels, dt_encoded);
                         break;
    case SType::INT8:    label_encode_fw<SType::INT8>(
                           col, dt_labels, dt_encoded, is_binomial
                         );
                         break;
    case SType::INT16:   label_encode_fw<SType::INT16>(
                           col, dt_labels, dt_encoded, is_binomial
                         );
                         break;
    case SType::INT32:   label_encode_fw<SType::INT32>(
                           col, dt_labels, dt_encoded, is_binomial
                          );
                         break;
    case SType::INT64:   label_encode_fw<SType::INT64>(
                           col, dt_labels, dt_encoded, is_binomial
                         );
                         break;
    case SType::FLOAT32: label_encode_fw<SType::FLOAT32>(
                           col, dt_labels, dt_encoded, is_binomial
                         );
                         break;
    case SType::FLOAT64: label_encode_fw<SType::FLOAT64>(
                           col, dt_labels, dt_encoded, is_binomial
                         );
                         break;
    case SType::STR32:   label_encode_str<uint32_t>(
                           col, dt_labels, dt_encoded, is_binomial
                         );
                         break;
    case SType::STR64:   label_encode_str<uint64_t>(
                           col, dt_labels, dt_encoded, is_binomial
                         );
                         break;

    default:             throw TypeError() << "Target column type `" << stype
                                           << "` is not supported";
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
static void label_encode_bool(const Column& col,
                              dtptr& dt_labels,
                              dtptr& dt_encoded)
{
  // If we only got NA's, return
  if (col.na_count() == col.nrows()) return;

  // Set up boolean labels and their corresponding ids.
  Column labels_col = Column::new_data_column(2, SType::BOOL);
  auto labels_data = static_cast<int8_t*>(labels_col.get_data_editable());
  labels_data[0] = 0;
  labels_data[1] = 1;

  Column ids_col = Column::new_data_column(2, SType::INT32);
  auto ids_data = static_cast<int32_t*>(ids_col.get_data_editable());
  ids_data[0] = 0;
  ids_data[1] = 1;

  dt_labels = dtptr(new DataTable({std::move(labels_col), std::move(ids_col)},
                                  {"label", "id"}));
  dt_encoded = dtptr(new DataTable({Column(col)}, DataTable::default_names));
}


} // namespace dt
