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
#include <unordered_map>
#include "csv/toa.h"
#include "python/_all.h"
#include "python/string.h"
#include "utils/parallel.h"
#include "column.h"
#include "datatablemodule.h"

static inline constexpr size_t id(SType st1, SType st2) {
  return static_cast<size_t>(st1) * DT_STYPES_COUNT + static_cast<size_t>(st2);
}

using castfn0 = void (*)(const void*, void*, size_t);
using castfn1 = void (*)(const void*, void*, size_t, const int32_t*);
using castfn2 = void (*)(const void*, void*, size_t, const RowIndex&);

static std::unordered_map<size_t, castfn0> castfns0;
static std::unordered_map<size_t, castfn1> castfns1;
static std::unordered_map<size_t, castfn2> castfns2;


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

  size_t cast_id = id(stype(), new_stype);
  const RowIndex& rowindex = this->rowindex();
  const void* in_data = this->data();
  void* out_data = res->data_w();

  if (rowindex) {
    if (rowindex.is_simple_slice() && castfns0.count(cast_id)) {
      in_data = static_cast<const void*>(
                  static_cast<const char*>(in_data) +
                  rowindex.slice_start() * elemsize());
      castfns0[cast_id](in_data, out_data, nrows);
      return res;
    }
    if (rowindex.isarr32() && castfns1.count(cast_id)) {
      const int32_t* indices = rowindex.indices32();
      castfns1[cast_id](in_data, out_data, nrows, indices);
      return res;
    }
    if (castfns2.count(cast_id)) {
      castfns2[cast_id](in_data, out_data, nrows, rowindex);
      return res;
    }
    // fall-through
  }
  if (castfns0.count(cast_id)) {
    std::unique_ptr<Column> tmpcol;
    if (rowindex) {
      tmpcol = std::unique_ptr<Column>(this->shallowcopy());
      tmpcol->reify();
      in_data = tmpcol->data();
    }
    castfns0[cast_id](in_data, out_data, nrows);
    return res;
  }
  if (castfns2.count(cast_id)) {
    castfns2[cast_id](in_data, out_data, nrows, rowindex);
    return res;
  }

  // Old casting
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
// Cast operators
//------------------------------------------------------------------------------

template <typename T>
static inline T copy_fw(T x) { return x; }

template <typename T, typename U>
static inline U cast_simple(T x) { return static_cast<U>(x); }

template <typename T, typename U>
static inline U cast_fw(T x) {
  return (std::is_same<T, U>::value) ||
         (std::is_floating_point<T>::value && std::is_floating_point<U>::value)
           ? static_cast<U>(x)
           : ISNA<T>(x)? GETNA<U>() : static_cast<U>(x);
}

template <typename T>
static inline int8_t cast_bool(T x) {
  return ISNA<T>(x)? GETNA<int8_t>() : (x != 0);
}

static inline PyObject* cast_bool_pyobj(int8_t x) {
  return ISNA<int8_t>(x)? py::None().release() : py::obool(x).release();
}

template <typename T>
static inline PyObject* cast_int_pyobj(T x) {
  return ISNA<T>(x)? py::None().release() : py::oint(x).release();
}

template <typename T>
static inline PyObject* cast_float_pyobj(T x) {
  return ISNA<T>(x)? py::None().release() : py::ofloat(x).release();
}

static inline PyObject* cast_pyobj_pyobj(PyObject* x) {
  return py::oobj(x).release();
}




//------------------------------------------------------------------------------
// Cast itererators
//------------------------------------------------------------------------------

// Standard parallel iterator for a fixed-width column without a rowindex
//
template <typename T, typename U, U(*CAST_OP)(T)>
static void cast_fw0(const void* in_data, void* out_data, size_t nrows)
{
  auto in  = static_cast<const T*>(in_data);
  auto out = static_cast<U*>(out_data);
  dt::run_parallel(
      [=](size_t start, size_t stop, size_t step) {
        for (size_t i = start; i < stop; i += step) {
          out[i] = CAST_OP(in[i]);
        }
      },
      nrows);
}


template <typename T, typename U, U(*CAST_OP)(T)>
static void cast_fw1(const void* in_data, void* out_data, size_t nrows,
                     const int32_t* indices)
{
  auto in  = static_cast<const T*>(in_data);
  auto out = static_cast<U*>(out_data);
  dt::run_parallel(
      [=](size_t start, size_t stop, size_t step) {
        for (size_t i = start; i < stop; i += step) {
          out[i] = CAST_OP(in[indices[i]]);
        }
      },
      nrows);
}


template <typename T, typename U, U(*CAST_OP)(T)>
static void cast_fw2(const void* in_data, void* out_data, size_t nrows,
                     const RowIndex& rowindex)
{
  auto in  = static_cast<const T*>(in_data);
  auto out = static_cast<U*>(out_data);
  dt::run_parallel(
      [=](size_t start, size_t stop, size_t step) {
        for (size_t i = start; i < stop; i += step) {
          out[i] = CAST_OP(in[rowindex[i]]);
        }
      },
      nrows);
}


template <typename T, PyObject* (*CAST_OP)(T)>
static void cast_pyo(const void* in_data, void* out_data, size_t nrows)
{
  auto in = static_cast<const T*>(in_data);
  auto out = static_cast<PyObject**>(out_data);
  for (size_t i = 0; i < nrows; ++i) {
    Py_DECREF(out[i]);
    out[i] = CAST_OP(in[i]);
  }
}



/*
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
*/



//------------------------------------------------------------------------------
// BoolColumn
//------------------------------------------------------------------------------

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
      toffsets[i] = offset ^ GETNA<T>();
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
      toffsets[i] = offset ^ GETNA<OT>();
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





//------------------------------------------------------------------------------
// RealColumn casts
//------------------------------------------------------------------------------

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
      toffsets[i] = offset ^ GETNA<OT>();
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
void RealColumn<T>::cast_into(StringColumn<uint32_t>* target) const {
  MemoryRange strbuf = real_str_cast_helper<T, uint32_t>(this, target);
  target->replace_buffer(target->data_buf(), std::move(strbuf));
}

template <typename T>
void RealColumn<T>::cast_into(StringColumn<uint64_t>* target) const {
  MemoryRange strbuf = real_str_cast_helper<T, uint64_t>(this, target);
  target->replace_buffer(target->data_buf(), std::move(strbuf));
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
      toffsets[i] = offset ^ GETNA<OT>();
    }
  }
  wb->finalize();
  return wb->get_mbuf();
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

void py::DatatableModule::init_casts()
{
  // cast_fw0: cast a fw column without rowindex
  // cast_fw1: cast a fw column with ARR32 rowindex
  // cast_fw2: cast a fw column with any rowindex (including none)
  constexpr SType bool8  = SType::BOOL;
  constexpr SType int8   = SType::INT8;
  constexpr SType int16  = SType::INT16;
  constexpr SType int32  = SType::INT32;
  constexpr SType int64  = SType::INT64;
  constexpr SType real32 = SType::FLOAT32;
  constexpr SType real64 = SType::FLOAT64;
  constexpr SType obj64  = SType::OBJ;

  // Trivial casts
  castfns0[id(bool8, bool8)]   = cast_fw0<int8_t,  int8_t,  copy_fw<int8_t>>;
  castfns0[id(int8, int8)]     = cast_fw0<int8_t,  int8_t,  copy_fw<int8_t>>;
  castfns0[id(int16, int16)]   = cast_fw0<int16_t, int16_t, copy_fw<int16_t>>;
  castfns0[id(int32, int32)]   = cast_fw0<int32_t, int32_t, copy_fw<int32_t>>;
  castfns0[id(int64, int64)]   = cast_fw0<int64_t, int64_t, copy_fw<int64_t>>;
  castfns0[id(real32, real32)] = cast_fw0<float,   float,   copy_fw<float>>;
  castfns0[id(real64, real64)] = cast_fw0<double,  double,  copy_fw<double>>;

  // Casts into bool8
  castfns0[id(int8, bool8)]   = cast_fw0<int8_t,  int8_t, cast_bool<int8_t>>;
  castfns0[id(int16, bool8)]  = cast_fw0<int16_t, int8_t, cast_bool<int16_t>>;
  castfns0[id(int32, bool8)]  = cast_fw0<int32_t, int8_t, cast_bool<int32_t>>;
  castfns0[id(int64, bool8)]  = cast_fw0<int64_t, int8_t, cast_bool<int64_t>>;
  castfns0[id(real32, bool8)] = cast_fw0<float,   int8_t, cast_bool<float>>;
  castfns0[id(real64, bool8)] = cast_fw0<double,  int8_t, cast_bool<double>>;

  // Casts into int8
  castfns0[id(bool8, int8)]   = cast_fw0<int8_t,  int8_t, cast_fw<int8_t, int8_t>>;
  castfns0[id(int16, int8)]   = cast_fw0<int16_t, int8_t, cast_fw<int16_t, int8_t>>;
  castfns0[id(int32, int8)]   = cast_fw0<int32_t, int8_t, cast_fw<int32_t, int8_t>>;
  castfns0[id(int64, int8)]   = cast_fw0<int64_t, int8_t, cast_fw<int64_t, int8_t>>;
  castfns0[id(real32, int8)]  = cast_fw0<float,   int8_t, cast_fw<float, int8_t>>;
  castfns0[id(real64, int8)]  = cast_fw0<double,  int8_t, cast_fw<double, int8_t>>;

  // Casts into int16
  castfns0[id(bool8, int16)]  = cast_fw0<int8_t,  int16_t, cast_fw<int8_t, int16_t>>;
  castfns0[id(int8, int16)]   = cast_fw0<int8_t,  int16_t, cast_fw<int8_t, int16_t>>;
  castfns0[id(int32, int16)]  = cast_fw0<int32_t, int16_t, cast_fw<int32_t, int16_t>>;
  castfns0[id(int64, int16)]  = cast_fw0<int64_t, int16_t, cast_fw<int64_t, int16_t>>;
  castfns0[id(real32, int16)] = cast_fw0<float,   int16_t, cast_fw<float, int16_t>>;
  castfns0[id(real64, int16)] = cast_fw0<double,  int16_t, cast_fw<double, int16_t>>;

  // Casts into int32
  castfns0[id(bool8, int32)]  = cast_fw0<int8_t,  int32_t, cast_fw<int8_t, int32_t>>;
  castfns0[id(int8, int32)]   = cast_fw0<int8_t,  int32_t, cast_fw<int8_t, int32_t>>;
  castfns0[id(int16, int32)]  = cast_fw0<int16_t, int32_t, cast_fw<int16_t, int32_t>>;
  castfns0[id(int64, int32)]  = cast_fw0<int64_t, int32_t, cast_fw<int64_t, int32_t>>;
  castfns0[id(real32, int32)] = cast_fw0<float,   int32_t, cast_fw<float, int32_t>>;
  castfns0[id(real64, int32)] = cast_fw0<double,  int32_t, cast_fw<double, int32_t>>;

  // Casts into int64
  castfns0[id(bool8, int64)]  = cast_fw0<int8_t,  int64_t, cast_fw<int8_t, int64_t>>;
  castfns0[id(int8, int64)]   = cast_fw0<int8_t,  int64_t, cast_fw<int8_t, int64_t>>;
  castfns0[id(int16, int64)]  = cast_fw0<int16_t, int64_t, cast_fw<int16_t, int64_t>>;
  castfns0[id(int32, int64)]  = cast_fw0<int32_t, int64_t, cast_fw<int32_t, int64_t>>;
  castfns0[id(real32, int64)] = cast_fw0<float,   int64_t, cast_fw<float, int64_t>>;
  castfns0[id(real64, int64)] = cast_fw0<double,  int64_t, cast_fw<double, int64_t>>;

  // Casts into real32
  castfns0[id(bool8, real32)]  = cast_fw0<int8_t,  float, cast_fw<int8_t, float>>;
  castfns0[id(int8, real32)]   = cast_fw0<int8_t,  float, cast_fw<int8_t, float>>;
  castfns0[id(int16, real32)]  = cast_fw0<int16_t, float, cast_fw<int16_t, float>>;
  castfns0[id(int32, real32)]  = cast_fw0<int32_t, float, cast_fw<int32_t, float>>;
  castfns0[id(int64, real32)]  = cast_fw0<int64_t, float, cast_fw<int64_t, float>>;
  castfns0[id(real64, real32)] = cast_fw0<double,  float, cast_simple<double, float>>;

  // Casts into real64
  castfns0[id(bool8, real64)]  = cast_fw0<int8_t,  double, cast_fw<int8_t, double>>;
  castfns0[id(int8, real64)]   = cast_fw0<int8_t,  double, cast_fw<int8_t, double>>;
  castfns0[id(int16, real64)]  = cast_fw0<int16_t, double, cast_fw<int16_t, double>>;
  castfns0[id(int32, real64)]  = cast_fw0<int32_t, double, cast_fw<int32_t, double>>;
  castfns0[id(int64, real64)]  = cast_fw0<int64_t, double, cast_fw<int64_t, double>>;
  castfns0[id(real32, real64)] = cast_fw0<float,   double, cast_simple<float, double>>;

  // Casts into obj64
  castfns0[id(bool8, obj64)]  = cast_pyo<int8_t,    cast_bool_pyobj>;
  castfns0[id(int8, obj64)]   = cast_pyo<int8_t,    cast_int_pyobj<int8_t>>;
  castfns0[id(int16, obj64)]  = cast_pyo<int16_t,   cast_int_pyobj<int16_t>>;
  castfns0[id(int32, obj64)]  = cast_pyo<int32_t,   cast_int_pyobj<int32_t>>;
  castfns0[id(int64, obj64)]  = cast_pyo<int64_t,   cast_int_pyobj<int64_t>>;
  castfns0[id(real32, obj64)] = cast_pyo<float,     cast_float_pyobj<float>>;
  castfns0[id(real64, obj64)] = cast_pyo<double,    cast_float_pyobj<double>>;
  castfns0[id(obj64, obj64)]  = cast_pyo<PyObject*, cast_pyobj_pyobj>;

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
