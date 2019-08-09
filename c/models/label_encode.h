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
#include <unordered_map>
#include <vector>
#include "datatable.h"
#include "wstringcol.h"
#include "parallel/api.h"
#include "parallel/shared_mutex.h"
#include "utils/exceptions.h"
#include "models/dt_ftrl_base.h"
namespace dt {


void label_encode(const OColumn&, dtptr&, dtptr&, bool is_binomial = false);


/**
 *  Create labels datatable from unordered map for fixed width columns.
 */
template <SType stype_from, SType stype_to>
dtptr create_dt_labels_fw(const std::unordered_map<promote<element_t<stype_from>>,
                                                   element_t<stype_to>>& labels_map)
{
  using Tfrom = element_t<stype_from>;
  using Tto = element_t<stype_to>;
  size_t nlabels = labels_map.size();
  OColumn labels_col = OColumn::new_data_column(stype_from, nlabels);
  OColumn ids_col = OColumn::new_data_column(stype_to, nlabels);

  auto labels_data = static_cast<Tfrom*>(labels_col->data_w());
  auto ids_data = static_cast<Tto*>(ids_col->data_w());

  for (auto& label : labels_map) {
    labels_data[label.second] = static_cast<Tfrom>(label.first);
    ids_data[label.second] = label.second;
  }
  return dtptr(new DataTable(
           {std::move(labels_col), std::move(ids_col)},
           {"label", "id"}
         ));
}

/**
 *  Create labels datatable from unordered map for string columns.
 */
template <typename T, SType stype_to>
dtptr create_dt_labels_str(const std::unordered_map<std::string, element_t<stype_to>>& labels_map) {
  size_t nlabels = labels_map.size();
  OColumn ids_col = OColumn::new_data_column(stype_to, nlabels);
  auto ids_data = static_cast<element_t<stype_to>*>(ids_col->data_w());
  dt::writable_string_col c_label_names(nlabels);
  dt::writable_string_col::buffer_impl<T> sb(c_label_names);
  sb.commit_and_start_new_chunk(0);

  size_t i = 0;
  for (const auto& label : labels_map) {
    sb.write(label.first);
    ids_data[i] = label.second;
    i++;
  }

  sb.order();
  sb.commit_and_start_new_chunk(nlabels);
  return dtptr(new DataTable(
           {std::move(c_label_names).to_ocolumn(), std::move(ids_col)},
           {"label", "id"}
         ));
}

/**
 *  Fill column with the sequential ids starting from `i0`.
 *  Used in the multinomial case when we encounter new labels.
 */
template <typename T>
void set_ids(OColumn& col, T i0) {
  auto data = static_cast<T*>(col->data_w());
  for (T i = 0; i < static_cast<T>(col.nrows()); ++i) {
    data[i] = i0 + i;
  }
}


/**
 *  Encode fixed width columns.
 */
template <SType stype_from, SType stype_to>
void label_encode_fw(const OColumn& ocol, dtptr& dt_labels, dtptr& dt_encoded) {
  using T_from = promote<element_t<stype_from>>;
  using T_to = element_t<stype_to>;
  const size_t nrows = ocol.nrows();

  OColumn outcol = OColumn::new_data_column(stype_to, nrows);
  auto outdata = static_cast<T_to*>(outcol->data_w());
  std::unordered_map<T_from, T_to> labels_map;
  dt::shared_mutex shmutex;

  dt::parallel_for_static(nrows, NThreads(dt::FtrlBase::get_nthreads(nrows)),
    [&](size_t irow) {
      T_from v;
      bool isna = ocol.get_element(irow, &v);
      if (isna) {
        outdata[irow] = GETNA<T_to>();
        return;
      }

      dt::shared_lock<dt::shared_mutex> lock(shmutex);
      if (labels_map.count(v)) {
        outdata[irow] = labels_map[v];
      } else {
        lock.exclusive_start();
        if (labels_map.count(v) == 0) {
          if (stype_to == SType::BOOL && labels_map.size() == 2) {
            throw ValueError() << "Target column for binomial problem cannot "
                                  "contain more than two labels";
          }
          size_t nlabels = labels_map.size();
          labels_map[v] = static_cast<T_to>(nlabels);
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
  dt_encoded = dtptr(new DataTable({std::move(outcol)}, {"label_id"}));
}


/**
 *  Encode string columns.
 */
template <typename U, SType stype_to>
void label_encode_str(const OColumn& ocol, dtptr& dt_labels, dtptr& dt_encoded) {
  using T_to = element_t<stype_to>;
  const size_t nrows = ocol.nrows();
  OColumn outcol = OColumn::new_data_column(stype_to, nrows);
  auto outdata = static_cast<T_to*>(outcol->data_w());
  std::unordered_map<std::string, T_to> labels_map;
  dt::shared_mutex shmutex;

  dt::parallel_for_static(nrows, NThreads(dt::FtrlBase::get_nthreads(nrows)),
    [&](size_t irow) {
      CString str;
      bool isna = ocol.get_element(irow, &str);
      if (isna || str.size == 0) {
        outdata[irow] = GETNA<T_to>();
        return;
      }

      std::string v(str.ch, static_cast<size_t>(str.size));

      dt::shared_lock<dt::shared_mutex> lock(shmutex);
      if (labels_map.count(v)) {
        outdata[irow] = labels_map[v];
      } else {
        lock.exclusive_start();
        if (labels_map.count(v) == 0) {
          if (stype_to == SType::BOOL && labels_map.size() == 2) {
            throw ValueError() << "Target column for binomial problem cannot "
                                  "contain more than two labels";
          }
          size_t nlabels = labels_map.size();
          labels_map[v] = static_cast<T_to>(nlabels);
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
  dt_encoded = dtptr(new DataTable({std::move(outcol)}, {"label_id"}));
}


} // namespace dt

#endif
