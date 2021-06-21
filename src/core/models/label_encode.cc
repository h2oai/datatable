//------------------------------------------------------------------------------
// Copyright 2018-2020 H2O.ai
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
    case SType::VOID:
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
    case SType::DATE32:
    case SType::INT32:   label_encode_fw<SType::INT32>(
                           col, dt_labels, dt_encoded, is_binomial
                          );
                         break;
    case SType::TIME64:
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
    sztvec keys{ 0 };
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


/**
 *  Add negative class label and save its model id.
 */
void add_negative_class(dtptr& dt_labels) {
  dt::writable_string_col c_labels(1);
  dt::writable_string_col::buffer_impl<uint32_t> sb(c_labels);
  sb.commit_and_start_new_chunk(0);
  sb.write("_negative_class");
  sb.order();
  sb.commit_and_start_new_chunk(1);

  Column c_ids = Column::new_data_column(1, SType::INT32);
  auto d_ids = static_cast<int32_t*>(c_ids.get_data_editable());
  d_ids[0] = 0;

  dtptr dt_nc = dtptr(
                  new DataTable(
                    {std::move(c_labels).to_ocolumn(), std::move(c_ids)},
                    dt_labels->get_names()
                  )
                );

  dt_labels->clear_key();

  // Increment all the exiting label ids, and then insert
  // a `_negative_class` label with the zero id.
  adjust_values<int32_t>(
    dt_labels->get_column(1),
    [](int32_t& value, size_t) {
      ++value;
    }
  );

  dt_labels->rbind({dt_nc.get()}, {{ 0 } , { 1 }});
  sztvec keys{ 0 };
  dt_labels->set_key(keys);
}


/**
 *  Convert target column to boolean type, and set up mapping
 *  between models and the incoming label inficators.
 */
void create_y_binomial(const DataTable* dt,
                       dtptr& dt_binomial,
                       std::vector<size_t>& label_ids,
                       dtptr& dt_labels) {
  xassert(label_ids.size() == 0);
  dtptr dt_labels_in;
  label_encode(dt->get_column(0), dt_labels_in, dt_binomial, true);

  // If we only got NA targets, return to stop training.
  if (dt_labels_in == nullptr) return;
  size_t nlabels_in = dt_labels_in->nrows();

  if (nlabels_in > 2) {
    throw ValueError() << "For binomial regression target column should have "
                       << "two labels at maximum, got: " << nlabels_in;
  }

  // By default we assume zero model got zero label id
  label_ids.push_back(0);

  if (dt_labels == nullptr) {
    dt_labels = std::move(dt_labels_in);
  } else {

    RowIndex ri_join = natural_join(*dt_labels_in.get(), *dt_labels.get());
    size_t nlabels = dt_labels->nrows();
    xassert(nlabels != 0 && nlabels < 3);
    auto data_label_ids_in = static_cast<int32_t*>(
                              dt_labels_in->get_column(1).get_data_editable());
    auto data_label_ids = static_cast<const int32_t*>(
                              dt_labels->get_column(1).get_data_readonly());

    size_t ri0_index = 0, ri1_index;
    bool ri0_valid = (ri_join.size() >= 1) && ri_join.get_element(0, &ri0_index);
    bool ri1_valid = (ri_join.size() >= 2) && ri_join.get_element(1, &ri1_index);

    switch (nlabels) {
      case 1: switch (nlabels_in) {
                 case 1: if (!ri0_valid) {
                           // The new label we got was encoded with zeros,
                           // so we train the model on all negatives: 1 == 0
                           label_ids[0] = 1;
                           data_label_ids_in[0] = 1;
                           // Since we cannot rbind anything to a keyed frame, we
                           // - clear the key;
                           // - rbind new labels;
                           // - set the key back, this will sort the resulting `dt_labels`.
                           dt_labels->clear_key();
                           dt_labels->rbind({ dt_labels_in.get() }, {{ 0 }, { 1 }});
                           sztvec keys{ 0 };
                           dt_labels->set_key(keys);
                         }
                         break;
                 case 2: if (!ri0_valid && !ri1_valid) {
                           throw ValueError() << "Got two new labels in the target column, "
                                              << "however, positive label is already set";
                         }
                         // If the new label is the zero label,
                         // then we need to train on the existing label indicator
                         // i.e. the first one.
                         label_ids[0] = static_cast<size_t>(data_label_ids_in[!ri0_valid]);
                         // Reverse labels id order if the new label comes first.
                         if (label_ids[0] == 1) {
                           data_label_ids_in[0] = 1;
                           data_label_ids_in[1] = 0;
                         }
                         dt_labels = std::move(dt_labels_in);
              }
              break;

      case 2: switch (nlabels_in) {
                case 1: if (!ri0_valid) {
                          throw ValueError() << "Got a new label in the target column, however, both "
                                             << "positive and negative labels are already set";
                        }
                        label_ids[0] = (data_label_ids[ri0_index] == 1);
                        break;
                case 2: if (!ri0_valid || !ri1_valid) {
                          throw ValueError() << "Got a new label in the target column, however, both "
                                             << "positive and negative labels are already set";
                        }
                        size_t label_id = static_cast<size_t>(data_label_ids[ri0_index] != 0);
                        label_ids[0] = static_cast<size_t>(data_label_ids_in[label_id]);

                        break;
              }
              break;
    }
  }
}


/**
 *  Encode target column with the integer labels, and set up mapping
 *  between models and the incoming label indicators.
 */
size_t create_y_multinomial(const DataTable* dt,
                          dtptr& dt_multinomial,
                          std::vector<size_t>& label_ids,
                          dtptr& dt_labels,
                          const bool negative_class,
                          bool validation /* = false */) {
  xassert(label_ids.size() == 0);
  dtptr dt_labels_in;
  label_encode(dt->get_column(0), dt_labels_in, dt_multinomial);

  // If we only got NA targets, return to stop training.
  if (dt_labels_in == nullptr) return 0;

  auto data_label_ids_in = static_cast<const int32_t*>(
                              dt_labels_in->get_column(1).get_data_readonly()
                           );
  size_t nlabels_in = dt_labels_in->nrows();

  // When we start training for the first time, all the incoming labels
  // become the model labels. Mapping is trivial in this case.
  if (dt_labels == nullptr) {
    dt_labels = std::move(dt_labels_in);
    if (negative_class) {
      add_negative_class(dt_labels);
      ++nlabels_in;
    }

    label_ids.resize(nlabels_in);
    for (size_t i = 0; i < nlabels_in; ++i) {
      label_ids[i] = i - negative_class;
    }

  } else {
    // When we already have some labels, and got new ones, we first
    // set up mapping in such a way, so that models will train
    // on all the negatives.
    auto data_label_ids = static_cast<const int32_t*>(
                            dt_labels->get_column(1).get_data_readonly()
                          );

    RowIndex ri_join = natural_join(*dt_labels_in.get(), *dt_labels.get());
    size_t nlabels = dt_labels->nrows();

    for (size_t i = 0; i < nlabels; ++i) {
      label_ids.push_back(std::numeric_limits<size_t>::max());
    }

    // Then we go through the list of new labels and relate existing models
    // to the incoming label indicators.
    Buffer new_label_indices = Buffer::mem(nlabels_in * sizeof(int64_t));
    int64_t* data = static_cast<int64_t*>(new_label_indices.xptr());
    size_t n_new_labels = 0;
    for (size_t i = 0; i < nlabels_in; ++i) {
      size_t rii;
      bool rii_valid = ri_join.get_element(i, &rii);
      size_t label_id_in = static_cast<size_t>(data_label_ids_in[i]);
      if (rii_valid) {
        size_t label_id = static_cast<size_t>(data_label_ids[rii]);
        label_ids[label_id] = label_id_in;
      } else {
        // If there is no corresponding label already set,
        // we will need to create a new one and its model.
        data[n_new_labels] = static_cast<int64_t>(i);
        label_ids.push_back(label_id_in);
        n_new_labels++;
      }
    }

    if (n_new_labels) {
      // In the case of validation we don't allow unseen labels.
      if (validation) {
        throw ValueError() << "Validation target column cannot contain labels, "
                           << "the model was not trained on";
      }

      // Extract new labels from `dt_labels_in`, and rbind them to `dt_labels`.
      new_label_indices.resize(n_new_labels * sizeof(int64_t),
                               true);  // keep data
      RowIndex ri_labels(std::move(new_label_indices), RowIndex::ARR64);
      dt_labels_in->apply_rowindex(ri_labels);

      // Set new ids for the incoming labels, so that they can be rbinded
      // to the existing ones. NB: this operation won't affect the relation
      // between the models and label indicators, because at this point
      // it has been already set in `label_ids`.
      adjust_values<int32_t>(
        dt_labels_in->get_column(1),
        [&] (int32_t& value, size_t irow) {
          value = static_cast<int32_t>(irow + dt_labels->nrows());
        }
      );

      // Since we cannot rbind anything to a keyed frame, we
      // - clear the key;
      // - rbind new labels;
      // - set the key back, this will sort the resulting `dt_labels`.
      dt_labels->clear_key();
      dt_labels->rbind({ dt_labels_in.get() }, {{ 0 } , { 1 }});
      sztvec keys{ 0 };
      dt_labels->set_key(keys);

      return n_new_labels;
    }

  }

  return 0;
}


} // namespace dt
