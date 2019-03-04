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

using castfn0 = void (*)(const Column*, size_t start, void* output);
using castfn1 = void (*)(const Column*, const int32_t* indices, void* output);
using castfn2 = void (*)(const Column*, void* output);
using castfnx = MemoryRange (*)(const void*, size_t, const RowIndex&);

static std::unordered_map<size_t, castfn0> castfns0;
static std::unordered_map<size_t, castfn1> castfns1;
static std::unordered_map<size_t, castfn2> castfns2;

static Column* cast_bool_to_str(const Column*, MemoryRange&&, SType);
static Column* cast_pyobj_to_str(const Column*, MemoryRange&&, SType);
template <typename T>
static Column* cast_num_to_str(const Column*, MemoryRange&&, SType);



//------------------------------------------------------------------------------
// Column (base methods)
//------------------------------------------------------------------------------

Column* Column::cast(SType new_stype) const {
  return cast(new_stype, MemoryRange());
}

Column* Column::cast(SType new_stype, MemoryRange&& mr) const
{
  if (new_stype == SType::STR32 || new_stype == SType::STR64) {
    switch (stype()) {
      case SType::BOOL: return cast_bool_to_str(this, std::move(mr), new_stype);
      case SType::INT8: return cast_num_to_str<int8_t>(this, std::move(mr), new_stype);
      case SType::INT16: return cast_num_to_str<int16_t>(this, std::move(mr), new_stype);
      case SType::INT32: return cast_num_to_str<int32_t>(this, std::move(mr), new_stype);
      case SType::INT64: return cast_num_to_str<int64_t>(this, std::move(mr), new_stype);
      case SType::FLOAT32: return cast_num_to_str<float>(this, std::move(mr), new_stype);
      case SType::FLOAT64: return cast_num_to_str<double>(this, std::move(mr), new_stype);
      case SType::OBJ: return cast_pyobj_to_str(this, std::move(mr), new_stype);
      default: break;
    }
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
  void* out_data = res->data_w();

  if (rowindex) {
    if (rowindex.is_simple_slice() && castfns0.count(cast_id)) {
      castfns0[cast_id](this, rowindex.slice_start(), out_data);
      return res;
    }
    if (rowindex.isarr32() && castfns1.count(cast_id)) {
      const int32_t* indices = rowindex.indices32();
      castfns1[cast_id](this, indices, out_data);
      return res;
    }
    if (castfns2.count(cast_id)) {
      castfns2[cast_id](this, out_data);
      return res;
    }
    // fall-through
  }
  if (castfns0.count(cast_id)) {
    std::unique_ptr<Column> tmpcol;
    if (rowindex) {
      tmpcol = std::unique_ptr<Column>(this->shallowcopy());
      tmpcol->reify();
      castfns0[cast_id](tmpcol.get(), 0, out_data);
    } else {
      castfns0[cast_id](this, 0, out_data);
    }
    return res;
  }
  if (castfns2.count(cast_id)) {
    castfns2[cast_id](this, out_data);
    return res;
  }

  // Old casting
  switch (new_stype) {
    case SType::STR32:   cast_into(static_cast<StringColumn<uint32_t>*>(res)); break;
    case SType::STR64:   cast_into(static_cast<StringColumn<uint64_t>*>(res)); break;
    case SType::OBJ:     cast_into(static_cast<PyObjectColumn*>(res)); break;
    default:
      throw ValueError() << "Unable to cast into stype = " << new_stype;
  }
  return res;
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
static inline T cast_copy(T x) {
  return x;
}

template <typename T, typename U>
static inline U cast_simple(T x) {
  return static_cast<U>(x);
}

template <typename T, typename U>
static inline U cast_fw(T x) {
  return ISNA<T>(x)? GETNA<U>() : static_cast<U>(x);
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

// Standard parallel iterator for a column without a rowindex, casting into a
// fixed-width column of type <U>. Parameter `start` allows the iteration to
// begin somewhere in the middle of the column's data (in support of column's
// with RowIndexes that are plain slices).
//
template <typename T, typename U, U(*CAST_OP)(T)>
static void cast_fw0(const Column* col, size_t start, void* out_data)
{
  xassert(col->rowindex()? col->rowindex().is_simple_slice()
                         : (start == 0));
  auto inp = static_cast<const T*>(col->data()) + start;
  auto out = static_cast<U*>(out_data);
  dt::run_parallel(
      [=](size_t i0, size_t i1, size_t di) {
        for (size_t i = i0; i < i1; i += di) {
          out[i] = CAST_OP(inp[i]);
        }
      },
      col->nrows);
}


template <typename T, typename U, U(*CAST_OP)(T)>
static void cast_fw1(const Column* col, const int32_t* indices,
                     void* out_data)
{
  auto inp = static_cast<const T*>(col->data());
  auto out = static_cast<U*>(out_data);
  dt::run_parallel(
      [=](size_t start, size_t stop, size_t step) {
        for (size_t i = start; i < stop; i += step) {
          out[i] = CAST_OP(inp[indices[i]]);
        }
      },
      col->nrows);
}


template <typename T, typename U, U(*CAST_OP)(T)>
static void cast_fw2(const Column* col, void* out_data)
{
  auto inp = static_cast<const T*>(col->data());
  auto out = static_cast<U*>(out_data);
  const RowIndex& rowindex = col->rowindex();
  dt::run_parallel(
      [=](size_t start, size_t stop, size_t step) {
        for (size_t i = start; i < stop; i += step) {
          out[i] = CAST_OP(inp[rowindex[i]]);
        }
      },
      col->nrows);
}


// Casting into PyObject* can only be done in single-threaded mode.
//
template <typename T, PyObject* (*CAST_OP)(T)>
static void cast_pyo(const Column* col, void* out_data)
{
  auto inp = static_cast<const T*>(col->data());
  auto out = static_cast<PyObject**>(out_data);
  const RowIndex& rowindex = col->rowindex();
  for (size_t i = 0; i < col->nrows; ++i) {
    Py_DECREF(out[i]);
    out[i] = CAST_OP(inp[rowindex[i]]);
  }
}



static Column* cast_bool_to_str(const Column* col, MemoryRange&& out_offsets,
                                SType target_stype)
{
  static CString str_na;
  static CString str_true  {"True", 4};
  static CString str_false {"False", 5};
  auto inp = static_cast<const int8_t*>(col->data());
  const RowIndex& rowindex = col->rowindex();
  return dt::generate_string_column(
      [&](size_t i, dt::string_buf* buf) {
        int8_t x = inp[rowindex[i]];
        buf->write(ISNA<int8_t>(x)? str_na : x? str_true : str_false);
      },
      col->nrows,
      std::move(out_offsets),
      (target_stype == SType::STR64)
  );
}


template <typename T>
static Column* cast_num_to_str(const Column* col, MemoryRange&& out_offsets,
                               SType target_stype)
{
  auto inp = static_cast<const T*>(col->data());
  const RowIndex& rowindex = col->rowindex();
  return dt::generate_string_column(
      [&](size_t i, dt::string_buf* buf) {
        T x = inp[rowindex[i]];
        if (ISNA<T>(x)) {
          buf->write_na();
        } else {
          char* ch = buf->prepare_raw_write(24);
          toa<T>(&ch, x);
          buf->commit_raw_write(ch);
        }
      },
      col->nrows,
      std::move(out_offsets),
      (target_stype == SType::STR64)
  );
}



static Column* cast_pyobj_to_str(const Column* col, MemoryRange&& out_offsets,
                                 SType target_stype)
{
  auto inp = static_cast<PyObject* const*>(col->data());
  const RowIndex& rowindex = col->rowindex();
  return dt::generate_string_column(
      [&](size_t i, dt::string_buf* buf) {
        PyObject* x = inp[rowindex[i]];
        CString xstr = py::robj(x).to_pystring_force().to_cstring();
        buf->write(xstr);
      },
      col->nrows,
      std::move(out_offsets),
      /* force_str64 = */ (target_stype == SType::STR64),
      /* force_single_threaded = */ true
  );
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
  castfns0[id(bool8, bool8)]   = cast_fw0<int8_t,  int8_t,  cast_copy<int8_t>>;
  castfns0[id(int8, int8)]     = cast_fw0<int8_t,  int8_t,  cast_copy<int8_t>>;
  castfns0[id(int16, int16)]   = cast_fw0<int16_t, int16_t, cast_copy<int16_t>>;
  castfns0[id(int32, int32)]   = cast_fw0<int32_t, int32_t, cast_copy<int32_t>>;
  castfns0[id(int64, int64)]   = cast_fw0<int64_t, int64_t, cast_copy<int64_t>>;
  castfns0[id(real32, real32)] = cast_fw0<float,   float,   cast_copy<float>>;
  castfns0[id(real64, real64)] = cast_fw0<double,  double,  cast_copy<double>>;

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
  castfns2[id(bool8, obj64)]  = cast_pyo<int8_t,    cast_bool_pyobj>;
  castfns2[id(int8, obj64)]   = cast_pyo<int8_t,    cast_int_pyobj<int8_t>>;
  castfns2[id(int16, obj64)]  = cast_pyo<int16_t,   cast_int_pyobj<int16_t>>;
  castfns2[id(int32, obj64)]  = cast_pyo<int32_t,   cast_int_pyobj<int32_t>>;
  castfns2[id(int64, obj64)]  = cast_pyo<int64_t,   cast_int_pyobj<int64_t>>;
  castfns2[id(real32, obj64)] = cast_pyo<float,     cast_float_pyobj<float>>;
  castfns2[id(real64, obj64)] = cast_pyo<double,    cast_float_pyobj<double>>;
  castfns2[id(obj64, obj64)]  = cast_pyo<PyObject*, cast_pyobj_pyobj>;

}




//------------------------------------------------------------------------------
// Explicit instantiation of templates
//------------------------------------------------------------------------------

template class StringColumn<uint32_t>;
template class StringColumn<uint64_t>;
