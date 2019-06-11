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
#include <vector>
#include "wstringcol.h"
#include "parallel/api.h"
#include "parallel/shared_mutex.h"
#include "utils/exceptions.h"
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
    std::vector<size_t> keys{ 0 };
    dt_labels->set_key({ keys });
  }
}


/**
 *  Encode fixed width columns.
 */
template <SType stype_from, SType stype_to>
void label_encode_fw(const Column* col, dtptr& dt_labels, dtptr& dt_encoded) {
  using T_from = element_t<stype_from>;
  using T_to = element_t<stype_to>;
  const size_t nrows = col->nrows;
  const RowIndex& ri = col->rowindex();

  colptr outcol = colptr(Column::new_data_column(stype_to, nrows));
  auto outdata = static_cast<T_to*>(outcol->data_w());
  std::unordered_map<T_from, T_to> labels_map;
  dt::shared_mutex shmutex;


  auto data = static_cast<const T_from*>(col->data());
  dt::parallel_for_static(nrows,
    [&](size_t irow) {
      size_t jrow = ri[irow];
      T_from v = data[jrow];

      if (jrow == RowIndex::NA || ISNA<T_from>(v)) {
        outdata[irow] = GETNA<T_to>();
        return;
      }

      dt::shared_lock<dt::shared_mutex> lock(shmutex);
      if (labels_map.count(v)) {
        outdata[irow] = labels_map[v];
      } else {
        lock.exclusive_start();
        if (stype_to == SType::BOOL && labels_map.size() == 2) {
          throw ValueError() << "Target column for binomial problem cannot "
                                "contain more than two labels";
        }

        if (labels_map.count(v) == 0) {
          labels_map[v] = static_cast<T_to>(labels_map.size());
          outdata[irow] = labels_map[v];
        } else {
          // In case the label was already added from another thread while
          // we were waiting for the exclusive lock
          outdata[irow] = labels_map[v];
        }
        lock.exclusive_end();
      }
    });

  // If we only got NA labels, return
  if (labels_map.size() == 0) return;

  dt_labels = create_dt_labels_fw<stype_from, stype_to>(labels_map);
  dt_encoded = dtptr(new DataTable({outcol->shallowcopy()}, {"label_id"}));
}


/**
 *  Encode string columns.
 */
template <typename U, SType stype_to>
void label_encode_str(const Column* col, dtptr& dt_labels, dtptr& dt_encoded) {
  // std::cout << "stype_to bool: " << (stype_to == SType::BOOL) << "\n";
  using T_to = element_t<stype_to>;
  const size_t nrows = col->nrows;
  const RowIndex& ri = col->rowindex();
  colptr outcol = colptr(Column::new_data_column(stype_to, nrows));
  auto outdata = static_cast<T_to*>(outcol->data_w());
  std::unordered_map<std::string, T_to> labels_map;
  dt::shared_mutex shmutex;

  auto scol = static_cast<const StringColumn<U>*>(col);
  const U* offsets = scol->offsets();
  const char* strdata = scol->strdata();

  dt::parallel_for_static(nrows,
    [&](size_t irow) {
      size_t jrow = ri[irow];

      if (jrow == RowIndex::NA || ISNA<U>(offsets[jrow])) {
        outdata[irow] = GETNA<T_to>();
        return;
      }

      const U strstart = offsets[jrow - 1] & ~GETNA<U>();
      auto len = offsets[jrow] - strstart;
      if (len == 0) {
        outdata[irow] = GETNA<T_to>();
        return;
      }

      const char* c_str = strdata + strstart;
      std::string v(c_str, len);

      dt::shared_lock<dt::shared_mutex> lock(shmutex);
      if (labels_map.count(v)) {
        outdata[irow] = labels_map[v];
      } else {
        lock.exclusive_start();
        if (stype_to == SType::BOOL && labels_map.size() == 2) {
          throw ValueError() << "Target column for binomial problem cannot "
                                "contain more than two labels";
        }

        if (labels_map.count(v) == 0) {
          labels_map[v] = static_cast<T_to>(labels_map.size());
          outdata[irow] = labels_map[v];
        } else {
          // In case the label was already added from another thread while
          // we were waiting for the exclusive lock
          outdata[irow] = labels_map[v];
        }
        lock.exclusive_end();
      }
    });

  // If we only got NA labels, return
  if (labels_map.size() == 0) return;

  dt_labels = create_dt_labels_str<U, stype_to>(labels_map);
  dt_encoded = dtptr(new DataTable({outcol->shallowcopy()}, {"label_id"}));
}


/**
 *  Create labels datatable from unordered map for fixed widht columns.
 */
template <SType stype_from, SType stype_to>
dtptr create_dt_labels_fw(const std::unordered_map<element_t<stype_from>, element_t<stype_to>>& labels_map) {
  size_t nlabels = labels_map.size();
  auto labels_col = Column::new_data_column(stype_from, nlabels);
  auto ids_col = Column::new_data_column(stype_to, nlabels);

  auto labels_data = static_cast<element_t<stype_from>*>(labels_col->data_w());
  auto ids_data = static_cast<element_t<stype_to>*>(ids_col->data_w());

  for (auto& label : labels_map) {
    labels_data[label.second] = label.first;
    ids_data[label.second] = label.second;
  }
  return dtptr(new DataTable(
           {labels_col, ids_col},
           {"label", "id"}
         ));
}


/**
 *  Create labels datatable from unordered map for string columns.
 */
template <typename T, SType stype_to>
dtptr create_dt_labels_str(const std::unordered_map<std::string, element_t<stype_to>>& labels_map) {
  size_t nlabels = labels_map.size();
  auto ids_col = Column::new_data_column(stype_to, nlabels);
  auto ids_data = static_cast<element_t<stype_to>*>(ids_col->data_w());
  dt::writable_string_col c_label_names(nlabels);
  dt::writable_string_col::buffer_impl<T> sb(c_label_names);
  sb.commit_and_start_new_chunk(0);

  size_t i = 0;
  for (const std::pair<std::string, element_t<stype_to>>& label : labels_map) {
    sb.write(label.first);
    ids_data[i] = label.second;
    i++;
  }

  sb.order();
  sb.commit_and_start_new_chunk(nlabels);
  return dtptr(new DataTable(
           {std::move(c_label_names).to_column(), ids_col},
           {"label", "id"}
         ));
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
