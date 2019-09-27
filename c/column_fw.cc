//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include <type_traits>
#include "utils/assert.h"
#include "utils/misc.h"
#include "parallel/api.h"  // dt::parallel_for_static
#include "column.h"
#include "column_impl.h"

template<> inline py::robj GETNA() { return py::rnone(); }

template <typename T> constexpr SType stype_for(){ return SType::VOID; }
template <> constexpr SType stype_for<int8_t>()  { return SType::INT8; }
template <> constexpr SType stype_for<int16_t>() { return SType::INT16; }
template <> constexpr SType stype_for<int32_t>() { return SType::INT32; }
template <> constexpr SType stype_for<int64_t>() { return SType::INT64; }
template <> constexpr SType stype_for<float>()   { return SType::FLOAT32; }
template <> constexpr SType stype_for<double>()  { return SType::FLOAT64; }



/**
 * Private constructor that creates an "invalid" column. An `init_X` method
 * should be subsequently called before using this column.
 */
template <typename T>
FwColumn<T>::FwColumn()
  : ColumnImpl(0, stype_for<T>()) {}


template <typename T>
FwColumn<T>::FwColumn(size_t nrows_)
  : ColumnImpl(nrows_, stype_for<T>())
{
  mbuf.resize(sizeof(T) * nrows_);
}


template <typename T>
FwColumn<T>::FwColumn(size_t nrows_, Buffer&& mr)
  : ColumnImpl(nrows_, stype_for<T>())
{
  size_t req_size = sizeof(T) * nrows_;
  if (mr) {
    xassert(mr.size() == req_size);
  } else {
    mr.resize(req_size);
  }
  mbuf = std::move(mr);
}


//==============================================================================
// Initialization methods
//==============================================================================

template <typename T>
void FwColumn<T>::init_data() {
  mbuf.resize(_nrows * sizeof(T));
}



//==============================================================================

template <typename T>
const T* FwColumn<T>::elements_r() const {
  return static_cast<const T*>(mbuf.rptr());
}

template <typename T>
T* FwColumn<T>::elements_w() {
  return static_cast<T*>(mbuf.wptr());
}


template <typename T>
T FwColumn<T>::get_elem(size_t i) const {
  return static_cast<const T*>(mbuf.rptr())[i];
}

template <typename T>
bool FwColumn<T>::get_element(size_t i, T* out) const {
  T x = static_cast<const T*>(mbuf.rptr())[i];
  *out = x;
  return !ISNA<T>(x);
}


template <typename T>
ColumnImpl* FwColumn<T>::materialize() {
  return this;
}





template <typename T>
size_t FwColumn<T>::data_nrows() const {
  return mbuf.size() / sizeof(T);
}


template <typename T>
void FwColumn<T>::apply_na_mask(const Column& mask) {
  xassert(mask.stype() == SType::BOOL);
  auto maskdata = static_cast<const int8_t*>(mask.get_data_readonly());
  T* coldata = this->elements_w();

  dt::parallel_for_static(_nrows,
    [=](size_t i) {
      if (maskdata[i] == 1) coldata[i] = GETNA<T>();
    });
  if (stats != nullptr) stats->reset();
}


template <typename T>
void FwColumn<T>::fill_na() {
  T* vals = static_cast<T*>(mbuf.wptr());
  dt::parallel_for_static(_nrows,
    [=](size_t i) {
      vals[i] = GETNA<T>();
    });
}


template <typename T>
void FwColumn<T>::replace_values(const RowIndex& replace_at, T replace_with) {
  T* data = elements_w();  // This will materialize the column if necessary
  replace_at.iterate(0, replace_at.size(), 1,
    [&](size_t, size_t j) {
      data[j] = replace_with;
    });
  if (stats) stats->reset();
}


template <typename T>
void FwColumn<T>::replace_values(
    Column&, const RowIndex& replace_at, const Column& replace_with)
{
  materialize();
  if (!replace_with) {
    return replace_values(replace_at, GETNA<T>());
  }
  Column with = (replace_with.stype() == _stype)
                    ? replace_with  // copy
                    : replace_with.cast(_stype);

  if (with.nrows() == 1) {
    T replace_value;
    bool isvalid = with.get_element(0, &replace_value);
    return isvalid? replace_values(replace_at, replace_value) :
                    replace_values(replace_at, GETNA<T>());
  }

  size_t replace_n = replace_at.size();
  T* data_dest = elements_w();
  xassert(with.nrows() == replace_n);

  replace_at.iterate(0, replace_n, 1,
    [&](size_t i, size_t j) {
      if (j == RowIndex::NA) return;
      T value;
      bool isvalid = replace_with.get_element(i, &value);
      data_dest[j] = isvalid? value : GETNA<T>();
    });
}




// Explicit instantiations
template class FwColumn<int8_t>;
template class FwColumn<int16_t>;
template class FwColumn<int32_t>;
template class FwColumn<int64_t>;
template class FwColumn<float>;
template class FwColumn<double>;
template class FwColumn<py::robj>;
