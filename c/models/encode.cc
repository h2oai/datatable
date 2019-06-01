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
#include <unordered_map>  // std::unordered_map
#include <vector>         // std::vector
#include "parallel/api.h"
#include "parallel/shared_mutex.h"
#include "utils/exceptions.h"
#include "datatable.h"
#include "models/encode.h"


namespace dt {


EncodedLabels encode(Column* col) {
  EncodedLabels res;
  SType stype = col->stype();
  switch (stype) {
    case SType::BOOL:    res = encode_bool(col); break;
    case SType::INT8:    res = encode_impl<SType::INT8>(col); break;
    case SType::INT16:   res = encode_impl<SType::INT16>(col); break;
    case SType::INT32:   res = encode_impl<SType::INT32>(col); break;
    case SType::INT64:   res = encode_impl<SType::INT64>(col); break;
    case SType::FLOAT32: res = encode_impl<SType::FLOAT32>(col); break;
    case SType::FLOAT64: res = encode_impl<SType::FLOAT64>(col); break;
    default:             throw TypeError() << "Column type `"
                                           << stype << "` is not supported";
  }
  return res;
}


template <SType stype>
EncodedLabels encode_impl(Column* col){
  EncodedLabels res;
  using T = element_t<stype>;
  size_t nrows = col->nrows;
  const RowIndex& ri = col->rowindex();
  auto data = static_cast<const T*>(col->data());
  auto outcol = new IntColumn<int32_t>(nrows);
  auto outdata = outcol->elements_w();

  std::unordered_map<T, int32_t> labels_map;
  dt::shared_mutex shmutex;


  dt::parallel_for_static(nrows,
    [&](size_t irow) {
      size_t jrow = ri[irow];
      T v = data[jrow];

      if (jrow == RowIndex::NA || ISNA<T>(v)) {
        outdata[irow] = GETNA<int32_t>();
        return;
      }

      dt::shared_lock<dt::shared_mutex> lock(shmutex);
      if (labels_map.count(v)) {
        outdata[irow] = labels_map[v];
      } else {
        lock.exclusive_start();
        if (labels_map.count(v) == 0) {
          labels_map[v] = static_cast<int32_t>(labels_map.size());
          outdata[irow] = labels_map[v];
        } else {
          // In case the label was already added from another thread while
          // we were waiting for the exclusive lock
          outdata[irow] = labels_map[v];
        }
        lock.exclusive_end();
      }
    });

  // If we only got NA labels, return {nullptr, nullptr}
  if (labels_map.size() == 0) return res;

  auto ids_col = Column::new_data_column(SType::INT32, nrows);
  auto labels_col = Column::new_data_column(stype, nrows);

  auto ids_data = static_cast<int32_t*>(ids_col->data_w());
  auto labels_data = static_cast<T*>(labels_col->data_w());

  for (auto& label : labels_map) {
    labels_data[label.second] = label.first;
    ids_data[label.second] = label.second;
  }

  res.dt_labels = dtptr(new DataTable({ids_col, labels_col}, {"id", "label"}));
  res.dt_encoded = dtptr(new DataTable({outcol}, {"label_id"}));

  return res;
}


EncodedLabels encode_bool(Column* col) {
  EncodedLabels res;
  auto data = static_cast<const int8_t*>(col->data());

  // If we only got NA's, return {nullptr, nullptr}
  for (size_t i = 0; i < col->nrows; ++i) {
    if (ISNA<int8_t>(data[i])) continue;
    break;
  }

  if

  auto ids_col = new IntColumn<int32_t>(2);
  auto labels_col = new BoolColumn(2);
  auto ids_data = ids_col->elements_w();
  auto labels_data = labels_col->elements_w();

  ids_data[0] = 0;
  ids_data[1] = 1;
  labels_data[0] = 0;
  labels_data[1] = 1;

  res.dt_labels = dtptr(new DataTable({ids_col, labels_col}, {"id", "label"}));
  res.dt_encoded = dtptr(new DataTable({col->shallowcopy()}));
  return res;
}


} // namespace dt
