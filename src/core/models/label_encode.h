//------------------------------------------------------------------------------
// Copyright 2018-2020 H2O.ai
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
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
#include "stype.h"
namespace dt {


void label_encode(const Column&, dtptr&, dtptr&, bool is_binomial = false);


/**
 *  Apply a function `adjustfn()` to all the column values.
 *  NB: this function doesn't do any NA checks and can only be applied
 *  to FW columns. It is used by FTRL to adjust label ids for the multinomial
 *  regression.
 */
template <typename T, typename F>
void adjust_values(Column& col, F adjustfn) {
  col.materialize();
  T* data = static_cast<T*>(col.get_data_editable());
  for (size_t i = 0; i < col.nrows(); ++i) {
    adjustfn(data[i], i);
  }
}


/**
 *  Create labels datatable from unordered map for fixed width columns.
 */
template <SType stype_from>
dtptr create_dt_labels_fw(const std::unordered_map<
                            element_t<stype_from>,
                            int32_t
                          >& labels_map)
{
  using T_from = element_t<stype_from>;
  size_t nlabels = labels_map.size();
  Column labels_col = Column::new_data_column(nlabels, stype_from);
  Column ids_col = Column::new_data_column(nlabels, SType::INT32);

  auto labels_data = static_cast<T_from*>(labels_col.get_data_editable());
  auto ids_data = static_cast<int32_t*>(ids_col.get_data_editable());

  for (auto& label : labels_map) {
    labels_data[label.second] = static_cast<T_from>(label.first);
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
template <typename T>
dtptr create_dt_labels_str(const std::unordered_map<
                             std::string,
                             int32_t
                           >& labels_map)
{
  size_t nlabels = labels_map.size();
  Column ids_col = Column::new_data_column(nlabels, SType::INT32);
  auto ids_data = static_cast<int32_t*>(
                    ids_col.get_data_editable()
                  );
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
 *  Encode fixed width columns.
 */
template <SType stype_from>
void label_encode_fw(const Column& ocol,
                     dtptr& dt_labels,
                     dtptr& dt_encoded,
                     bool is_binomial)
{
  using T_from = element_t<stype_from>;
  const size_t nrows = ocol.nrows();

  Column outcol = Column::new_data_column(nrows, SType::INT32);
  auto outdata = static_cast<int32_t*>(outcol.get_data_editable());
  std::unordered_map<T_from, int32_t> labels_map;
  dt::shared_mutex shmutex;
  NThreads nthreads = nthreads_from_niters(nrows, dt::FtrlBase::MIN_ROWS_PER_THREAD,
                                           ocol.allow_parallel_access());

  dt::parallel_for_static(nrows, nthreads,
    [&](size_t irow) {
      T_from v;
      bool isvalid = ocol.get_element(irow, &v);
      if (!isvalid) {
        outdata[irow] = GETNA<int32_t>();
        return;
      }

      dt::shared_lock<dt::shared_mutex> lock(shmutex);
      if (labels_map.count(v)) {
        outdata[irow] = labels_map[v];
      } else {
        lock.exclusive_start();
        if (labels_map.count(v) == 0) {
          if (is_binomial && labels_map.size() == 2) {
            throw ValueError() << "Target column for binomial problem cannot "
                                  "contain more than two unique labels";
          }
          size_t nlabels = labels_map.size();
          labels_map[v] = static_cast<int32_t>(nlabels);
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

  dt_labels = create_dt_labels_fw<stype_from>(labels_map);
  dt_encoded = dtptr(new DataTable({std::move(outcol)}, {"label_id"}));
}


/**
 *  Encode string columns.
 */
template <typename T>
void label_encode_str(const Column& ocol,
                      dtptr& dt_labels,
                      dtptr& dt_encoded,
                      bool is_binomial)
{
  const size_t nrows = ocol.nrows();
  Column outcol = Column::new_data_column(nrows, SType::INT32);
  auto outdata = static_cast<int32_t*>(outcol.get_data_editable());
  std::unordered_map<std::string, int32_t> labels_map;
  dt::shared_mutex shmutex;
  NThreads nthreads = nthreads_from_niters(nrows, dt::FtrlBase::MIN_ROWS_PER_THREAD,
                                           ocol.allow_parallel_access());

  dt::parallel_for_static(nrows, nthreads,
    [&](size_t irow) {
      CString cstr;
      bool isvalid = ocol.get_element(irow, &cstr);
      if (!isvalid || cstr.size() == 0) {
        outdata[irow] = GETNA<int32_t>();
        return;
      }

      auto v = cstr.to_string();

      dt::shared_lock<dt::shared_mutex> lock(shmutex);
      if (labels_map.count(v)) {
        outdata[irow] = labels_map[v];
      } else {
        lock.exclusive_start();
        if (labels_map.count(v) == 0) {
          if (is_binomial && labels_map.size() == 2) {
            throw ValueError() << "Target column for binomial problem cannot "
                                  "contain more than two unique labels";
          }
          size_t nlabels = labels_map.size();
          labels_map[v] = static_cast<int32_t>(nlabels);
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

  dt_labels = create_dt_labels_str<T>(labels_map);
  dt_encoded = dtptr(new DataTable({std::move(outcol)}, {"label_id"}));
}




} // namespace dt
#endif
