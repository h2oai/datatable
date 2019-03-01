//------------------------------------------------------------------------------
// Copyright 2018 H2O.ai
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
#include "csv/toa.h"
#include "python/_all.h"
#include "python/string.h"
#include "utils/parallel.h"
#include "column.h"
#include "datatablemodule.h"

using castfn = dt::function<void(const Column* incol, Column* outcol)>;
static castfn cast_fns[DT_STYPES_COUNT * DT_STYPES_COUNT];

static inline constexpr size_t id(SType st1, SType st2) {
  return static_cast<size_t>(st1) * DT_STYPES_COUNT + static_cast<size_t>(st2);
}



//------------------------------------------------------------------------------
// Column (base methods)
//------------------------------------------------------------------------------

Column* Column::cast(SType new_stype) const {
  return cast(new_stype, MemoryRange());
}

Column* Column::cast(SType new_stype, MemoryRange&& mr) const {
  if (ri) {
    // TODO: implement this
    throw RuntimeError() << "Cannot cast a column with rowindex";
  }
  Column *res = nullptr;
  if (mr) {
    res = Column::new_column(new_stype);
    res->nrows = nrows;
    res->mbuf = std::move(mr);
  } else {
    if (new_stype == stype()) {
      return shallowcopy();
    }
    res = Column::new_data_column(new_stype, nrows);
  }
  {
    auto fn = cast_fns[id(stype(), new_stype)];
    if (fn) {
      fn(this, res);
      return res;
    }
  }
  switch (new_stype) {
    case SType::BOOL:    cast_into(static_cast<BoolColumn*>(res)); break;
    case SType::INT8:    cast_into(static_cast<IntColumn<int8_t>*>(res)); break;
    case SType::INT16:   cast_into(static_cast<IntColumn<int16_t>*>(res)); break;
    case SType::INT32:   cast_into(static_cast<IntColumn<int32_t>*>(res)); break;
    case SType::INT64:   cast_into(static_cast<IntColumn<int64_t>*>(res)); break;
    case SType::FLOAT32: cast_into(static_cast<RealColumn<float>*>(res)); break;
    case SType::FLOAT64: cast_into(static_cast<RealColumn<double>*>(res)); break;
    case SType::STR32:   cast_into(static_cast<StringColumn<uint32_t>*>(res)); break;
    case SType::STR64:   cast_into(static_cast<StringColumn<uint64_t>*>(res)); break;
    case SType::OBJ:     cast_into(static_cast<PyObjectColumn*>(res)); break;
    default:
      throw ValueError() << "Unable to cast into stype = " << new_stype;
  }
  return res;
}

void Column::cast_into(BoolColumn*) const {
  throw ValueError() << "Cannot cast " << stype() << " into bool";
}
void Column::cast_into(IntColumn<int8_t>*) const {
  throw ValueError() << "Cannot cast " << stype() << " into int8";
}
void Column::cast_into(IntColumn<int16_t>*) const {
  throw ValueError() << "Cannot cast " << stype() << " into int16";
}
void Column::cast_into(IntColumn<int32_t>*) const {
  throw ValueError() << "Cannot cast " << stype() << " into int32";
}
void Column::cast_into(IntColumn<int64_t>*) const {
  throw ValueError() << "Cannot cast " << stype() << " into int64";
}
void Column::cast_into(RealColumn<float>*) const {
  throw ValueError() << "Cannot cast " << stype() << " into float";
}
void Column::cast_into(RealColumn<double>*) const {
  throw ValueError() << "Cannot cast " << stype() << " into double";
}
void Column::cast_into(StringColumn<uint32_t>*) const {
  throw ValueError() << "Cannot cast " << stype() << " into str32";
}
void Column::cast_into(StringColumn<uint64_t>*) const {
  throw ValueError() << "Cannot cast " << stype() << " into str64";
}
void Column::cast_into(PyObjectColumn*) const {
  throw ValueError() << "Cannot cast " << stype() << " into pyobj";
}



//------------------------------------------------------------------------------
// Fixed-width casts
//------------------------------------------------------------------------------

template <typename T, typename U>
static inline U fw_cast(T x) {
  return ISNA<T>(x)? GETNA<U>() : static_cast<U>(x);
}

template <typename T>
static inline int8_t bool_cast(T x) {
  return ISNA<T>(x)? GETNA<int8_t>() : (x != 0);
}


// Cast fixed-width column from T into U
template <typename T, typename U, U(*CAST_OP)(T)>
static void fwcast(const Column* incol, Column* outcol) {
  const RowIndex& rowindex = incol->rowindex();
  const T* in_data = static_cast<const T*>(incol->data());
  U* out_data = static_cast<U*>(outcol->data_w());

  if (rowindex) {
    if (rowindex.is_simple_slice()) {
      in_data += rowindex.slice_start();
      goto norowindex;
    }
    dt::run_parallel(
      [=](size_t start, size_t stop, size_t step) {
        for (size_t i = start; i < stop; i += step) {
          // call to rowindex[] virtual function
          out_data[i] = CAST_OP(in_data[rowindex[i]]);
        }
      },
      incol->nrows);
  }
  else {
    norowindex:
    dt::run_parallel(
      [=](size_t start, size_t stop, size_t step) {
        for (size_t i = start; i < stop; i += step) {
          out_data[i] = CAST_OP(in_data[i]);
        }
      },
      incol->nrows);
  }
}


template <typename T>
static void cast_to_str(const Column* incol, Column* outcol) {
  static CString str_na;
  static CString str_true  {"True", 4};
  static CString str_false {"False", 5};
  const RowIndex& rowindex = incol->rowindex();
  const T* in_data = static_cast<const T*>(incol->data());
  dt::map_fw2str(
    [](size_t i, fhbuf& buf) {
      T x = in_data[rowindex[i]];
      buf.write(ISNA<T>(x)? str_na : x? str_true : str_false);
    }, incol->nrows);
}




//------------------------------------------------------------------------------
// BoolColumn
//------------------------------------------------------------------------------

template <typename T>
inline static void bool_cast_helper(size_t nrows, const int8_t* src, T* trg) {
  #pragma omp parallel for schedule(static)
  for (size_t i = 0; i < nrows; ++i) {
    int8_t x = src[i];
    trg[i] = ISNA<int8_t>(x)? GETNA<T>() : static_cast<T>(x);
  }
}

template <typename T>
inline static MemoryRange bool_str_cast_helper(
  const BoolColumn* src, StringColumn<T>* target)
{
  const int8_t* src_data = src->elements_r();
  T* toffsets = target->offsets_w();
  size_t exp_size = src->nrows;
  auto wb = make_unique<MemoryWritableBuffer>(exp_size);
  char* tmpbuf = new char[1024];
  TRACK(tmpbuf, sizeof(tmpbuf), "BoolColumn::tmpbuf");
  char* tmpend = tmpbuf + 1000;  // Leave at least 24 spare chars in buffer
  char* ch = tmpbuf;
  T offset = 0;
  toffsets[-1] = 0;
  for (size_t i = 0; i < src->nrows; ++i) {
    int8_t x = src_data[i];
    if (ISNA<int8_t>(x)) {
      toffsets[i] = offset | GETNA<T>();
    } else {
      *ch++ = '0' + x;
      offset++;
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
  UNTRACK(tmpbuf);
  return wb->get_mbuf();
}


void BoolColumn::cast_into(BoolColumn* target) const {
  std::memcpy(target->data_w(), data(), alloc_size());
}

void BoolColumn::cast_into(IntColumn<int8_t>* target) const {
  std::memcpy(target->data_w(), data(), alloc_size());
}

void BoolColumn::cast_into(IntColumn<int16_t>* target) const {
  bool_cast_helper<int16_t>(nrows, this->elements_r(), target->elements_w());
}

void BoolColumn::cast_into(IntColumn<int32_t>* target) const {
  bool_cast_helper<int32_t>(nrows, this->elements_r(), target->elements_w());
}

void BoolColumn::cast_into(IntColumn<int64_t>* target) const {
  bool_cast_helper<int64_t>(nrows, this->elements_r(), target->elements_w());
}

void BoolColumn::cast_into(RealColumn<float>* target) const {
  bool_cast_helper<float>(nrows, this->elements_r(), target->elements_w());
}

void BoolColumn::cast_into(RealColumn<double>* target) const {
  bool_cast_helper<double>(nrows, this->elements_r(), target->elements_w());
}

void BoolColumn::cast_into(PyObjectColumn* target) const {
  const int8_t* src_data = this->elements_r();
  PyObject**    trg_data = target->elements_w();
  for (size_t i = 0; i < nrows; ++i) {
    trg_data[i] = py::obool(src_data[i]).release();
  }
}

void BoolColumn::cast_into(StringColumn<uint32_t>* target) const {
  MemoryRange strbuf = bool_str_cast_helper<uint32_t>(this, target);
  target->replace_buffer(target->data_buf(), std::move(strbuf));
}

void BoolColumn::cast_into(StringColumn<uint64_t>* target) const {
  MemoryRange strbuf = bool_str_cast_helper<uint64_t>(this, target);
  target->replace_buffer(target->data_buf(), std::move(strbuf));
}



//------------------------------------------------------------------------------
// IntColumn casts
//------------------------------------------------------------------------------

template<typename IT, typename OT>
inline static void int_cast_helper(size_t nrows, const IT* src, OT* trg) {
  #pragma omp parallel for schedule(static)
  for (size_t i = 0; i < nrows; ++i) {
    IT x = src[i];
    trg[i] = ISNA<IT>(x)? GETNA<OT>() : static_cast<OT>(x);
  }
}

template<typename IT, typename OT>
inline static MemoryRange int_str_cast_helper(
  size_t nrows, const IT* src, OT* toffsets)
{
  size_t exp_size = nrows * sizeof(IT);
  auto wb = make_unique<MemoryWritableBuffer>(exp_size);
  char* tmpbuf = new char[1024];
  TRACK(tmpbuf, sizeof(tmpbuf), "IntColumn::tmpbuf");
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
  UNTRACK(tmpbuf);
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
  int_cast_helper<T, int8_t>(this->nrows, this->elements_r(), target->elements_w());
}

template <typename T>
void IntColumn<T>::cast_into(IntColumn<int16_t>* target) const {
  int_cast_helper<T, int16_t>(this->nrows, this->elements_r(), target->elements_w());
}

template <typename T>
void IntColumn<T>::cast_into(IntColumn<int32_t>* target) const {
  int_cast_helper<T, int32_t>(this->nrows, this->elements_r(), target->elements_w());
}

template <typename T>
void IntColumn<T>::cast_into(IntColumn<int64_t>* target) const {
  int_cast_helper<T, int64_t>(this->nrows, this->elements_r(), target->elements_w());
}

template <typename T>
void IntColumn<T>::cast_into(RealColumn<float>* target) const {
  int_cast_helper<T, float>(this->nrows, this->elements_r(), target->elements_w());
}

template <typename T>
void IntColumn<T>::cast_into(RealColumn<double>* target) const {
  int_cast_helper<T, double>(this->nrows, this->elements_r(), target->elements_w());
}

template <typename T>
void IntColumn<T>::cast_into(StringColumn<uint32_t>* target) const {
  uint32_t* offsets = target->offsets_w();
  MemoryRange strbuf = int_str_cast_helper<T, uint32_t>(
      this->nrows, this->elements_r(), offsets
  );
  target->replace_buffer(target->data_buf(), std::move(strbuf));
}

template <typename T>
void IntColumn<T>::cast_into(StringColumn<uint64_t>* target) const {
  uint64_t* offsets = target->offsets_w();
  MemoryRange strbuf = int_str_cast_helper<T, uint64_t>(
      this->nrows, this->elements_r(), offsets
  );
  target->replace_buffer(target->data_buf(), std::move(strbuf));
}

template <typename T>
void IntColumn<T>::cast_into(PyObjectColumn* target) const {
  const T* src_data = this->elements_r();
  PyObject** trg_data = target->elements_w();
  for (size_t i = 0; i < this->nrows; ++i) {
    T x = src_data[i];
    trg_data[i] = ISNA<T>(x) ? py::None().release() : py::oint(x).release();
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
// RealColumn casts
//------------------------------------------------------------------------------

template<typename IT, typename OT>
inline static void real_cast_helper(size_t nrows, const IT* src, OT* trg) {
  #pragma omp parallel for schedule(static)
  for (size_t i = 0; i < nrows; ++i) {
    IT x = src[i];
    trg[i] = ISNA<IT>(x)? GETNA<OT>() : static_cast<OT>(x);
  }
}

template<typename IT, typename OT>
inline static MemoryRange real_str_cast_helper(
  const RealColumn<IT>* src, StringColumn<OT>* target)
{
  const IT* src_data = src->elements_r();
  OT* toffsets = target->offsets_w();
  size_t exp_size = src->nrows * sizeof(IT) * 2;
  auto wb = make_unique<MemoryWritableBuffer>(exp_size);
  char* tmpbuf = new char[1024];
  TRACK(tmpbuf, sizeof(tmpbuf), "RealColumn::tmpbuf");
  char* tmpend = tmpbuf + 1000;  // Leave at least 24 spare chars in buffer
  char* ch = tmpbuf;
  OT offset = 0;
  toffsets[-1] = 0;
  for (size_t i = 0; i < src->nrows; ++i) {
    IT x = src_data[i];
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
  UNTRACK(tmpbuf);
  return wb->get_mbuf();
}


template <typename T>
void RealColumn<T>::cast_into(BoolColumn* target) const {
  constexpr int8_t na_trg = GETNA<int8_t>();
  const T* src_data = this->elements_r();
  int8_t* trg_data = target->elements_w();
  #pragma omp parallel for schedule(static)
  for (size_t i = 0; i < this->nrows; ++i) {
    T x = src_data[i];
    trg_data[i] = ISNA<T>(x)? na_trg : (x != 0);
  }
}

template <typename T>
void RealColumn<T>::cast_into(IntColumn<int8_t>* target) const {
  real_cast_helper<T, int8_t>(this->nrows, this->elements_r(), target->elements_w());
}

template <typename T>
void RealColumn<T>::cast_into(IntColumn<int16_t>* target) const {
  real_cast_helper<T, int16_t>(this->nrows, this->elements_r(), target->elements_w());
}

template <typename T>
void RealColumn<T>::cast_into(IntColumn<int32_t>* target) const {
  real_cast_helper<T, int32_t>(this->nrows, this->elements_r(), target->elements_w());
}

template <typename T>
void RealColumn<T>::cast_into(IntColumn<int64_t>* target) const {
  real_cast_helper<T, int64_t>(this->nrows, this->elements_r(), target->elements_w());
}

template <typename T>
void RealColumn<T>::cast_into(StringColumn<uint32_t>* target) const {
  MemoryRange strbuf = real_str_cast_helper<T, uint32_t>(this, target);
  target->replace_buffer(target->data_buf(), std::move(strbuf));
}

template <typename T>
void RealColumn<T>::cast_into(StringColumn<uint64_t>* target) const {
  MemoryRange strbuf = real_str_cast_helper<T, uint64_t>(this, target);
  target->replace_buffer(target->data_buf(), std::move(strbuf));
}

template <>
void RealColumn<float>::cast_into(RealColumn<double>* target) const {
  const float* src_data = this->elements_r();
  double* trg_data = target->elements_w();
  #pragma omp parallel for schedule(static)
  for (size_t i = 0; i < this->nrows; ++i) {
    trg_data[i] = static_cast<double>(src_data[i]);
  }
}

template <>
void RealColumn<double>::cast_into(RealColumn<float>* target) const {
  const double* src_data = this->elements_r();
  float* trg_data = target->elements_w();
  #pragma omp parallel for schedule(static)
  for (size_t i = 0; i < this->nrows; ++i) {
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
  for (size_t i = 0; i < this->nrows; ++i) {
    T x = src_data[i];
    trg_data[i] = ISNA<T>(x)? py::None().release()
                            : py::ofloat(x).release();
  }
}




//------------------------------------------------------------------------------
// StringColumn casts
//------------------------------------------------------------------------------

template <typename T>
void StringColumn<T>::cast_into(PyObjectColumn* target) const {
  const char* strdata = this->strdata();
  const T* offsets = this->offsets();
  PyObject** trg_data = target->elements_w();

  T prev_offset = 0;
  for (size_t i = 0; i < this->nrows; ++i) {
    T off_end = offsets[i];
    if (ISNA<T>(off_end)) {
      trg_data[i] = py::None().release();
    } else {
      trg_data[i] = py::ostring(strdata + prev_offset,
                    off_end - prev_offset).release();
      prev_offset = off_end;
    }
  }
}


template <>
void StringColumn<uint32_t>::cast_into(StringColumn<uint64_t>* target) const {
  const uint32_t* src_data = this->offsets();
  uint64_t* trg_data = target->offsets_w();
  uint64_t dNA = GETNA<uint64_t>() - GETNA<uint32_t>();
  trg_data[-1] = 0;
  #pragma omp parallel for schedule(static)
  for (size_t i = 0; i < this->nrows; ++i) {
    uint32_t v = src_data[i];
    trg_data[i] = ISNA<uint32_t>(v)? v + dNA : v;
  }
  target->replace_buffer(target->data_buf(), MemoryRange(strbuf));
}


template <>
void StringColumn<uint64_t>::cast_into(StringColumn<uint64_t>* target) const {
  size_t alloc_size = sizeof(uint64_t) * (1 + this->nrows);
  std::memcpy(target->data_w(), this->data(), alloc_size);
  target->replace_buffer(target->data_buf(), MemoryRange(strbuf));
}




//------------------------------------------------------------------------------
// PyObjectColumn casts
//------------------------------------------------------------------------------

template<typename OT>
inline static MemoryRange obj_str_cast_helper(
  size_t nrows, const PyObject* const* src, OT* toffsets)
{
  // Warning: Do not attempt to parallelize this: creating new PyObjects
  // is not thread-safe! In addition `to_pystring_force()` may invoke
  // arbitrary python code when stringifying a value...
  size_t exp_size = nrows * sizeof(PyObject*);
  auto wb = make_unique<MemoryWritableBuffer>(exp_size);
  OT offset = 0;
  toffsets[-1] = 0;
  for (size_t i = 0; i < nrows; ++i) {
    py::ostring xstr = py::robj(src[i]).to_pystring_force();
    CString xcstr = xstr.to_cstring();
    if (xcstr.ch) {
      wb->write(static_cast<size_t>(xcstr.size), xcstr.ch);
      offset += static_cast<OT>(xcstr.size);
      toffsets[i] = offset;
    } else {
      toffsets[i] = offset | GETNA<OT>();
    }
  }
  wb->finalize();
  return wb->get_mbuf();
}


void PyObjectColumn::cast_into(PyObjectColumn* target) const {
  PyObject* const* src_data = this->elements_r();
  PyObject** dest_data = target->elements_w();
  for (size_t i = 0; i < nrows; ++i) {
    Py_INCREF(src_data[i]);
    Py_DECREF(dest_data[i]);
    dest_data[i] = src_data[i];
  }
}


void PyObjectColumn::cast_into(StringColumn<uint32_t>* target) const {
  const PyObject* const* data = elements_r();
  uint32_t* offsets = target->offsets_w();
  MemoryRange strbuf = obj_str_cast_helper<uint32_t>(nrows, data, offsets);
  target->replace_buffer(target->data_buf(), std::move(strbuf));
}


void PyObjectColumn::cast_into(StringColumn<uint64_t>* target) const {
  const PyObject* const* data = elements_r();
  uint64_t* offsets = target->offsets_w();
  MemoryRange strbuf = obj_str_cast_helper<uint64_t>(nrows, data, offsets);
  target->replace_buffer(target->data_buf(), std::move(strbuf));
}




//------------------------------------------------------------------------------
// One-time initialization
//------------------------------------------------------------------------------

void py::DatatableModule::init_casts() {
  std::memset(cast_fns, 0, sizeof(cast_fns));

  constexpr SType bool8 = SType::BOOL;
  constexpr SType int8 = SType::INT8;
  constexpr SType int16 = SType::INT16;
  constexpr SType int32 = SType::INT32;
  constexpr SType int64 = SType::INT64;
  constexpr SType float32 = SType::FLOAT32;
  constexpr SType float64 = SType::FLOAT64;

  cast_fns[id(bool8, bool8)]   = fwcast<int8_t, int8_t,  bool_cast<int8_t>>;
  cast_fns[id(bool8, int8)]    = fwcast<int8_t, int8_t,  fw_cast<int8_t, int8_t>>;
  cast_fns[id(bool8, int16)]   = fwcast<int8_t, int16_t, fw_cast<int8_t, int16_t>>;
  cast_fns[id(bool8, int32)]   = fwcast<int8_t, int32_t, fw_cast<int8_t, int32_t>>;
  cast_fns[id(bool8, int64)]   = fwcast<int8_t, int64_t, fw_cast<int8_t, int64_t>>;
  cast_fns[id(bool8, float32)] = fwcast<int8_t, float,   fw_cast<int8_t, float>>;
  cast_fns[id(bool8, float64)] = fwcast<int8_t, double,  fw_cast<int8_t, double>>;

  cast_fns[id(int8, bool8)]   = fwcast<int8_t, int8_t,  bool_cast<int8_t>>;
  cast_fns[id(int8, int8)]    = fwcast<int8_t, int8_t,  fw_cast<int8_t, int8_t>>;
  cast_fns[id(int8, int16)]   = fwcast<int8_t, int16_t, fw_cast<int8_t, int16_t>>;
  cast_fns[id(int8, int32)]   = fwcast<int8_t, int32_t, fw_cast<int8_t, int32_t>>;
  cast_fns[id(int8, int64)]   = fwcast<int8_t, int64_t, fw_cast<int8_t, int64_t>>;
  cast_fns[id(int8, float32)] = fwcast<int8_t, float,   fw_cast<int8_t, float>>;
  cast_fns[id(int8, float64)] = fwcast<int8_t, double,  fw_cast<int8_t, double>>;
}




//------------------------------------------------------------------------------
// Explicit instantiation of templates
//------------------------------------------------------------------------------

template class IntColumn<int8_t>;
template class IntColumn<int16_t>;
template class IntColumn<int32_t>;
template class IntColumn<int64_t>;
template class RealColumn<float>;
template class RealColumn<double>;
template class StringColumn<uint32_t>;
template class StringColumn<uint64_t>;
