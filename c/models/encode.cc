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
#include <vector>         // std::vector
#include "parallel/api.h"
#include "parallel/shared_mutex.h"
#include "utils/exceptions.h"
#include "datatable.h"
#include "models/encode.h"
#include <type_traits>
#include <typeinfo>
#include "wstringcol.h"


namespace dt {


EncodedLabels encode(Column* col, bool cast_to_bool /* = false =*/) {
  EncodedLabels res;
  SType stype = col->stype();
  switch (stype) {
    case SType::BOOL:    res = encode_bool(col); break;
    case SType::INT8:    res = encode_fw<SType::INT8>(col, cast_to_bool); break;
    case SType::INT16:   res = encode_fw<SType::INT16>(col, cast_to_bool); break;
    case SType::INT32:   res = encode_fw<SType::INT32>(col, cast_to_bool); break;
    case SType::INT64:   res = encode_fw<SType::INT64>(col, cast_to_bool); break;
    case SType::FLOAT32: res = encode_fw<SType::FLOAT32>(col, cast_to_bool); break;
    case SType::FLOAT64: res = encode_fw<SType::FLOAT64>(col, cast_to_bool); break;
    case SType::STR32:   res = encode_str<uint32_t>(col, cast_to_bool); break;
    case SType::STR64:   res = encode_str<uint64_t>(col, cast_to_bool); break;
    default:             throw TypeError() << "Column type `"
                                           << stype << "` is not supported";
  }

  std::vector<size_t> keys{ 0 };
  res.dt_labels->set_key({ keys });

  return res;
}


template <SType stype>
EncodedLabels encode_fw(Column* col, bool cast_to_bool){
  EncodedLabels res;
  using T = element_t<stype>;
  size_t nrows = col->nrows;
  const RowIndex& ri = col->rowindex();
  colptr outcol = colptr(new IntColumn<int32_t>(nrows));
  auto outdata = static_cast<int32_t*>(outcol->data_w());
  std::unordered_map<T, int32_t> labels_map;
  dt::shared_mutex shmutex;


  auto data = static_cast<const T*>(col->data());
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
  if (cast_to_bool && labels_map.size() < 3) outcol = colptr(outcol->cast(SType::BOOL));

  res.dt_labels = create_dt_labels_fw<stype>(labels_map);
  res.dt_encoded = dtptr(new DataTable({outcol->shallowcopy()}, {"label_id"}));
  return res;
}


template <typename U>
EncodedLabels encode_str(Column* col, bool cast_to_bool){
  EncodedLabels res;
  size_t nrows = col->nrows;
  const RowIndex& ri = col->rowindex();
  colptr outcol = colptr(new IntColumn<int32_t>(nrows));
  auto outdata = static_cast<int32_t*>(outcol->data_w());
  std::unordered_map<std::string, int32_t> labels_map;
  dt::shared_mutex shmutex;


  // using U = typename std::conditional<stype == SType::STR32, uint32_t, uint64_t>::type;
  auto scol = static_cast<StringColumn<U>*>(col);
  const U* offsets = scol->offsets();
  const char* strdata = scol->strdata();

  dt::parallel_for_static(nrows,
    [&](size_t irow) {
      size_t jrow = ri[irow];

      if (jrow == RowIndex::NA || ISNA<U>(offsets[jrow])) {
        outdata[irow] = GETNA<int32_t>();
        return;
      }

      const U strstart = offsets[jrow - 1] & ~GETNA<U>();
      auto len = offsets[jrow] - strstart;
      if (len == 0) {
        outdata[irow] = GETNA<int32_t>();
        return;
      }

      const char* c_str = strdata + strstart;
      std::string v(c_str, len);


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
  if (cast_to_bool && labels_map.size() < 3) {
    outcol = colptr(outcol->cast(SType::BOOL));
  }

  res.dt_labels = create_dt_labels_str<U>(labels_map);
  res.dt_encoded = dtptr(new DataTable({outcol->shallowcopy()}, {"label_id"}));

  return res;
}


template <SType stype>
dtptr create_dt_labels_fw(const std::unordered_map<element_t<stype>, int32_t>& labels_map) {
  size_t nlabels = labels_map.size();
  auto ids_col = Column::new_data_column(SType::INT32, nlabels);
  auto labels_col = Column::new_data_column(stype, nlabels);

  auto ids_data = static_cast<int32_t*>(ids_col->data_w());
  auto labels_data = static_cast<element_t<stype>*>(labels_col->data_w());

  for (auto& label : labels_map) {
    labels_data[label.second] = label.first;
    ids_data[label.second] = label.second;
  }
  return dtptr(new DataTable({labels_col, ids_col}, {"label", "id"}));
}


template <typename T>
dtptr create_dt_labels_str(const std::unordered_map<std::string, int32_t>& labels_map) {
  size_t nlabels = labels_map.size();
  auto ids_col = Column::new_data_column(SType::INT32, nlabels);
  auto ids_data = static_cast<int32_t*>(ids_col->data_w());
  dt::writable_string_col c_label_names(nlabels);
  dt::writable_string_col::buffer_impl<T> sb(c_label_names);
  sb.commit_and_start_new_chunk(0);

  size_t i = 0;
  for (const std::pair<std::string, int32_t>& label : labels_map) {
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


EncodedLabels encode_bool(Column* col) {
  EncodedLabels res;
  auto data = static_cast<const int8_t*>(col->data());

  // If we only got NA's, return {nullptr, nullptr}
  bool nas_only = true;
  for (size_t i = 0; i < col->nrows; ++i) {
    if (!ISNA<int8_t>(data[i])) {
      nas_only = false;
      break;
    }
  }

  if (nas_only) return res;

  auto ids_col = new IntColumn<int32_t>(2);
  auto labels_col = new BoolColumn(2);
  auto ids_data = ids_col->elements_w();
  auto labels_data = labels_col->elements_w();

  ids_data[0] = 0;
  ids_data[1] = 1;
  labels_data[0] = 0;
  labels_data[1] = 1;

  res.dt_labels = dtptr(new DataTable({labels_col, ids_col}, {"label", "id"}));
  res.dt_encoded = dtptr(new DataTable({col->shallowcopy()}));
  return res;
}


} // namespace dt
