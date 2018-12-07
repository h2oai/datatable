//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "column.h"
#include "csv/toa.h"
#include "python/int.h"
#include "py_types.h"
#include "py_utils.h"
#include "utils/parallel.h"



template <typename T>
SType IntColumn<T>::stype() const noexcept {
  return sizeof(T) == 1? SType::INT8 :
         sizeof(T) == 2? SType::INT16 :
         sizeof(T) == 4? SType::INT32 :
         sizeof(T) == 8? SType::INT64 : SType::VOID;
}

template <typename T>
py::oobj IntColumn<T>::get_value_at_index(size_t i) const {
  size_t j = (this->ri)[i];
  T x = this->elements_r()[j];
  return ISNA<T>(x)? py::None() : py::oint(x);
}



//------------------------------------------------------------------------------
// Stats
//------------------------------------------------------------------------------

template <typename T>
IntegerStats<T>* IntColumn<T>::get_stats() const {
  if (stats == nullptr) stats = new IntegerStats<T>();
  return static_cast<IntegerStats<T>*>(stats);
}

template <typename T> T       IntColumn<T>::min() const  { return get_stats()->min(this); }
template <typename T> T       IntColumn<T>::max() const  { return get_stats()->max(this); }
template <typename T> T       IntColumn<T>::mode() const { return get_stats()->mode(this); }
template <typename T> int64_t IntColumn<T>::sum() const  { return get_stats()->sum(this); }
template <typename T> double  IntColumn<T>::mean() const { return get_stats()->mean(this); }
template <typename T> double  IntColumn<T>::sd() const   { return get_stats()->stdev(this); }
template <typename T> double  IntColumn<T>::skew() const { return get_stats()->skew(this); }
template <typename T> double  IntColumn<T>::kurt() const { return get_stats()->kurt(this); }

// Retrieve stat value as a column
template <typename T>
Column* IntColumn<T>::min_column() const {
  IntColumn<T>* col = new IntColumn<T>(1);
  col->set_elem(0, min());
  return col;
}

template <typename T>
Column* IntColumn<T>::max_column() const {
  IntColumn<T>* col = new IntColumn<T>(1);
  col->set_elem(0, max());
  return col;
}

template <typename T>
Column* IntColumn<T>::mode_column() const {
  IntColumn<T>* col = new IntColumn<T>(1);
  col->set_elem(0, mode());
  return col;
}

template <typename T>
Column* IntColumn<T>::sum_column() const {
  IntColumn<int64_t>* col = new IntColumn<int64_t>(1);
  col->set_elem(0, sum());
  return col;
}

template <typename T>
Column* IntColumn<T>::mean_column() const {
  RealColumn<double>* col = new RealColumn<double>(1);
  col->set_elem(0, mean());
  return col;
}

template <typename T>
Column* IntColumn<T>::sd_column() const {
  RealColumn<double>* col = new RealColumn<double>(1);
  col->set_elem(0, sd());
  return col;
}

template <typename T>
Column* IntColumn<T>::skew_column() const {
  RealColumn<double>* col = new RealColumn<double>(1);
  col->set_elem(0, skew());
  return col;
}

template <typename T>
Column* IntColumn<T>::kurt_column() const {
  RealColumn<double>* col = new RealColumn<double>(1);
  col->set_elem(0, kurt());
  return col;
}

template <typename T>
int64_t IntColumn<T>::min_int64() const {
  T x = min();
  return ISNA<T>(x)? GETNA<int64_t>() : static_cast<int64_t>(x);
}

template <typename T>
int64_t IntColumn<T>::max_int64() const {
  T x = max();
  return ISNA<T>(x)? GETNA<int64_t>() : static_cast<int64_t>(x);
}


template <typename T> PyObject* IntColumn<T>::min_pyscalar() const { return int_to_py(min()); }
template <typename T> PyObject* IntColumn<T>::max_pyscalar() const { return int_to_py(max()); }
template <typename T> PyObject* IntColumn<T>::mode_pyscalar() const { return int_to_py(mode()); }
template <typename T> PyObject* IntColumn<T>::sum_pyscalar() const { return int_to_py(sum()); }
template <typename T> PyObject* IntColumn<T>::mean_pyscalar() const { return float_to_py(mean()); }
template <typename T> PyObject* IntColumn<T>::sd_pyscalar() const { return float_to_py(sd()); }
template <typename T> PyObject* IntColumn<T>::skew_pyscalar() const { return float_to_py(skew()); }
template <typename T> PyObject* IntColumn<T>::kurt_pyscalar() const { return float_to_py(kurt()); }




//------------------------------------------------------------------------------
// Type casts
//------------------------------------------------------------------------------
typedef std::unique_ptr<MemoryWritableBuffer> MWBPtr;

template<typename IT, typename OT>
inline static void cast_helper(size_t nrows, const IT* src, OT* trg) {
  #pragma omp parallel for schedule(static)
  for (size_t i = 0; i < nrows; ++i) {
    IT x = src[i];
    trg[i] = ISNA<IT>(x)? GETNA<OT>() : static_cast<OT>(x);
  }
}

template<typename IT, typename OT>
inline static MemoryRange cast_str_helper(
  size_t nrows, const IT* src, OT* toffsets)
{
  size_t exp_size = nrows * sizeof(IT);
  auto wb = MWBPtr(new MemoryWritableBuffer(exp_size));
  char* tmpbuf = new char[1024];
  char* tmpend = tmpbuf + 1000;  // Leave at least 24 spare chars in buffer
  char* ch = tmpbuf;
  OT offset = 0;
  toffsets[-1] = 0;
  for (size_t i = 0; i < nrows; ++i) {
    IT x = src[i];
    if (ISNA<IT>(x)) {
      toffsets[i] = offset | GETNA<OT>();
    } else {
      char* ch0 = ch;
      toa<IT>(&ch, x);
      offset += static_cast<OT>(ch - ch0);
      toffsets[i] = offset;
      if (ch > tmpend) {
        wb->write(static_cast<size_t>(ch - tmpbuf), tmpbuf);
        ch = tmpbuf;
      }
    }
  }
  wb->write(static_cast<size_t>(ch - tmpbuf), tmpbuf);
  wb->finalize();
  delete[] tmpbuf;
  return wb->get_mbuf();
}


template <typename T>
void IntColumn<T>::cast_into(BoolColumn* target) const {
  constexpr T na_src = GETNA<T>();
  constexpr int8_t na_trg = GETNA<int8_t>();
  const T* src_data = this->elements_r();
  int8_t* trg_data = target->elements_w();
  #pragma omp parallel for schedule(static)
  for (size_t i = 0; i < this->nrows; ++i) {
    T x = src_data[i];
    trg_data[i] = x == na_src? na_trg : (x != 0);
  }
}

template <typename T>
void IntColumn<T>::cast_into(IntColumn<int8_t>* target) const {
  cast_helper<T, int8_t>(this->nrows, this->elements_r(), target->elements_w());
}

template <typename T>
void IntColumn<T>::cast_into(IntColumn<int16_t>* target) const {
  cast_helper<T, int16_t>(this->nrows, this->elements_r(), target->elements_w());
}

template <typename T>
void IntColumn<T>::cast_into(IntColumn<int32_t>* target) const {
  cast_helper<T, int32_t>(this->nrows, this->elements_r(), target->elements_w());
}

template <typename T>
void IntColumn<T>::cast_into(IntColumn<int64_t>* target) const {
  cast_helper<T, int64_t>(this->nrows, this->elements_r(), target->elements_w());
}

template <typename T>
void IntColumn<T>::cast_into(RealColumn<float>* target) const {
  cast_helper<T, float>(this->nrows, this->elements_r(), target->elements_w());
}

template <typename T>
void IntColumn<T>::cast_into(RealColumn<double>* target) const {
  cast_helper<T, double>(this->nrows, this->elements_r(), target->elements_w());
}

template <typename T>
void IntColumn<T>::cast_into(StringColumn<uint32_t>* target) const {
  uint32_t* offsets = target->offsets_w();
  MemoryRange strbuf = cast_str_helper<T, uint32_t>(
      this->nrows, this->elements_r(), offsets
  );
  target->replace_buffer(target->data_buf(), std::move(strbuf));
}

template <typename T>
void IntColumn<T>::cast_into(StringColumn<uint64_t>* target) const {
  uint64_t* offsets = target->offsets_w();
  MemoryRange strbuf = cast_str_helper<T, uint64_t>(
      this->nrows, this->elements_r(), offsets
  );
  target->replace_buffer(target->data_buf(), std::move(strbuf));
}

template <typename T>
void IntColumn<T>::cast_into(PyObjectColumn* target) const {
  constexpr T na_src = GETNA<T>();
  const T* src_data = this->elements_r();
  PyObject** trg_data = target->elements_w();
  for (size_t i = 0; i < this->nrows; ++i) {
    T x = src_data[i];
    // PyLong_FromInt64 is declared in "py_types.h" as an alias for either
    // PyLong_FromLong or PyLong_FromLongLong, depending on the platform
    trg_data[i] = x == na_src ? none() :
                  (sizeof(T) == 8? PyLong_FromInt64(x) :
                                   PyLong_FromLong(static_cast<long>(x)));
  }
}

template <>
void IntColumn<int8_t>::cast_into(IntColumn<int8_t>* target) const {
  std::memcpy(target->data_w(), this->data(), alloc_size());
}

template <>
void IntColumn<int16_t>::cast_into(IntColumn<int16_t>* target) const {
  std::memcpy(target->data_w(), this->data(), alloc_size());
}

template <>
void IntColumn<int32_t>::cast_into(IntColumn<int32_t>* target) const {
  std::memcpy(target->data_w(), this->data(), alloc_size());
}

template <>
void IntColumn<int64_t>::cast_into(IntColumn<int64_t>* target) const {
  std::memcpy(target->data_w(), this->data(), alloc_size());
}



//------------------------------------------------------------------------------

// Explicit instantiation of the template
template class IntColumn<int8_t>;
template class IntColumn<int16_t>;
template class IntColumn<int32_t>;
template class IntColumn<int64_t>;
