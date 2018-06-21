//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018
//------------------------------------------------------------------------------
#include "column.h"
#include "csv/toa.h"
#include "utils/omp.h"
#include "py_utils.h"



template <typename T>
SType RealColumn<T>::stype() const {
  return stype_real(sizeof(T));
}



//------------------------------------------------------------------------------
// Stats
//------------------------------------------------------------------------------

template <typename T>
RealStats<T>* RealColumn<T>::get_stats() const {
  if (stats == nullptr) stats = new RealStats<T>();
  return static_cast<RealStats<T>*>(stats);
}

template <typename T> T      RealColumn<T>::min() const  { return get_stats()->min(this); }
template <typename T> T      RealColumn<T>::max() const  { return get_stats()->max(this); }
template <typename T> T      RealColumn<T>::mode() const  { return get_stats()->mode(this); }
template <typename T> double RealColumn<T>::sum() const  { return get_stats()->sum(this); }
template <typename T> double RealColumn<T>::mean() const { return get_stats()->mean(this); }
template <typename T> double RealColumn<T>::sd() const   { return get_stats()->stdev(this); }
template <typename T> double RealColumn<T>::skew() const { return get_stats()->skew(this); }
template <typename T> double RealColumn<T>::kurt() const   { return get_stats()->kurt(this); }

// Retrieve stat value as a column
template <typename T>
Column* RealColumn<T>::min_column() const {
  RealColumn<T>* col = new RealColumn<T>(1);
  col->set_elem(0, min());
  return col;
}

template <typename T>
Column* RealColumn<T>::max_column() const {
  RealColumn<T>* col = new RealColumn<T>(1);
  col->set_elem(0, max());
  return col;
}

template <typename T>
Column* RealColumn<T>::mode_column() const {
  RealColumn<T>* col = new RealColumn<T>(1);
  col->set_elem(0, mode());
  return col;
}

template <typename T>
Column* RealColumn<T>::sum_column() const {
  RealColumn<double>* col = new RealColumn<double>(1);
  col->set_elem(0, sum());
  return col;
}

template <typename T>
Column* RealColumn<T>::mean_column() const {
  RealColumn<double>* col = new RealColumn<double>(1);
  col->set_elem(0, mean());
  return col;
}

template <typename T>
Column* RealColumn<T>::sd_column() const {
  RealColumn<double>* col = new RealColumn<double>(1);
  col->set_elem(0, sd());
  return col;
}

template <typename T>
Column* RealColumn<T>::skew_column() const {
  RealColumn<double>* col = new RealColumn<double>(1);
  col->set_elem(0, skew());
  return col;
}

template <typename T>
Column* RealColumn<T>::kurt_column() const {
  RealColumn<double>* col = new RealColumn<double>(1);
  col->set_elem(0, kurt());
  return col;
}

template <typename T> PyObject* RealColumn<T>::min_pyscalar() const { return float_to_py(min()); }
template <typename T> PyObject* RealColumn<T>::max_pyscalar() const { return float_to_py(max()); }
template <typename T> PyObject* RealColumn<T>::mode_pyscalar() const { return float_to_py(mode()); }
template <typename T> PyObject* RealColumn<T>::sum_pyscalar() const { return float_to_py(sum()); }
template <typename T> PyObject* RealColumn<T>::mean_pyscalar() const { return float_to_py(mean()); }
template <typename T> PyObject* RealColumn<T>::sd_pyscalar() const { return float_to_py(sd()); }
template <typename T> PyObject* RealColumn<T>::skew_pyscalar() const { return float_to_py(skew()); }
template <typename T> PyObject* RealColumn<T>::kurt_pyscalar() const { return float_to_py(kurt()); }


//------------------------------------------------------------------------------
// Type casts
//------------------------------------------------------------------------------
typedef std::unique_ptr<MemoryWritableBuffer> MWBPtr;

template<typename IT, typename OT>
inline static void cast_helper(int64_t nrows, const IT* src, OT* trg) {
  #pragma omp parallel for schedule(static)
  for (int64_t i = 0; i < nrows; ++i) {
    IT x = src[i];
    trg[i] = ISNA<IT>(x)? GETNA<OT>() : static_cast<OT>(x);
  }
}

template<typename IT, typename OT>
inline static MemoryRange cast_str_helper(
  const RealColumn<IT>* src, StringColumn<OT>* target)
{
  const IT* src_data = src->elements_r();
  OT* toffsets = target->offsets_w();
  size_t exp_size = static_cast<size_t>(src->nrows) * sizeof(IT) * 2;
  auto wb = MWBPtr(new MemoryWritableBuffer(exp_size));
  char* tmpbuf = new char[1024];
  char* tmpend = tmpbuf + 1000;  // Leave at least 24 spare chars in buffer
  char* ch = tmpbuf;
  OT offset = 1;
  toffsets[-1] = -1;
  for (int64_t i = 0; i < src->nrows; ++i) {
    IT x = src_data[i];
    if (ISNA<IT>(x)) {
      toffsets[i] = -offset;
    } else {
      char* ch0 = ch;
      toa<IT>(&ch, x);
      offset += ch - ch0;
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
void RealColumn<T>::cast_into(BoolColumn* target) const {
  constexpr int8_t na_trg = GETNA<int8_t>();
  const T* src_data = this->elements_r();
  int8_t* trg_data = target->elements_w();
  #pragma omp parallel for schedule(static)
  for (int64_t i = 0; i < this->nrows; ++i) {
    T x = src_data[i];
    trg_data[i] = ISNA<T>(x)? na_trg : (x != 0);
  }
}

template <typename T>
void RealColumn<T>::cast_into(IntColumn<int8_t>* target) const {
  cast_helper<T, int8_t>(this->nrows, this->elements_r(), target->elements_w());
}

template <typename T>
void RealColumn<T>::cast_into(IntColumn<int16_t>* target) const {
  cast_helper<T, int16_t>(this->nrows, this->elements_r(), target->elements_w());
}

template <typename T>
void RealColumn<T>::cast_into(IntColumn<int32_t>* target) const {
  cast_helper<T, int32_t>(this->nrows, this->elements_r(), target->elements_w());
}

template <typename T>
void RealColumn<T>::cast_into(IntColumn<int64_t>* target) const {
  cast_helper<T, int64_t>(this->nrows, this->elements_r(), target->elements_w());
}

template <typename T>
void RealColumn<T>::cast_into(StringColumn<uint32_t>* target) const {
  MemoryRange strbuf = cast_str_helper<T, uint32_t>(this, target);
  target->replace_buffer(target->data_buf(), std::move(strbuf));
}

template <typename T>
void RealColumn<T>::cast_into(StringColumn<uint64_t>* target) const {
  MemoryRange strbuf = cast_str_helper<T, uint64_t>(this, target);
  target->replace_buffer(target->data_buf(), std::move(strbuf));
}

template <>
void RealColumn<float>::cast_into(RealColumn<double>* target) const {
  const float* src_data = this->elements_r();
  double* trg_data = target->elements_w();
  #pragma omp parallel for schedule(static)
  for (int64_t i = 0; i < this->nrows; ++i) {
    trg_data[i] = static_cast<double>(src_data[i]);
  }
}

template <>
void RealColumn<double>::cast_into(RealColumn<float>* target) const {
  const double* src_data = this->elements_r();
  float* trg_data = target->elements_w();
  #pragma omp parallel for schedule(static)
  for (int64_t i = 0; i < this->nrows; ++i) {
    trg_data[i] = static_cast<float>(src_data[i]);
  }
}

template <>
void RealColumn<float>::cast_into(RealColumn<float>* target) const {
  std::memcpy(target->data_w(), this->data(), alloc_size());
}

template <>
void RealColumn<double>::cast_into(RealColumn<double>* target) const {
  std::memcpy(target->data_w(), this->data(), alloc_size());
}

template <typename T>
void RealColumn<T>::cast_into(PyObjectColumn* target) const {
  const T* src_data = this->elements_r();
  PyObject** trg_data = target->elements_w();
  for (int64_t i = 0; i < this->nrows; ++i) {
    T x = src_data[i];
    trg_data[i] = ISNA<T>(x)? none()
                            : PyFloat_FromDouble(static_cast<double>(x));
  }
}




//------------------------------------------------------------------------------


// Explicit instantiation of the template
template class RealColumn<float>;
template class RealColumn<double>;
