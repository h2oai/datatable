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
using castfnx = Column* (*)(const Column*, MemoryRange&&, SType);

static std::unordered_map<size_t, castfn0> castfns0;
static std::unordered_map<size_t, castfn1> castfns1;
static std::unordered_map<size_t, castfn2> castfns2;
static std::unordered_map<size_t, castfnx> castfnsx;



//------------------------------------------------------------------------------
// Column (base methods)
//------------------------------------------------------------------------------

Column* Column::cast(SType new_stype) const {
  return cast(new_stype, MemoryRange());
}

Column* Column::cast(SType new_stype, MemoryRange&& mr) const
{
  size_t cast_id = id(stype(), new_stype);
  if (castfnsx.count(cast_id)) {
    return castfnsx[cast_id](this, std::move(mr), new_stype);
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

  throw ValueError()
      << "Unable to cast `" << stype() << "` into `" << new_stype << "`";
}




//------------------------------------------------------------------------------
// Cast operators
//------------------------------------------------------------------------------

template <typename T>
static inline T _copy(T x) {
  return x;
}

template <typename T, typename U>
static inline U _static(T x) {
  return static_cast<U>(x);
}

template <typename T, typename U>
static inline U fw_fw(T x) {
  return ISNA<T>(x)? GETNA<U>() : static_cast<U>(x);
}

template <typename T>
static inline int8_t fw_bool(T x) {
  return ISNA<T>(x)? GETNA<int8_t>() : (x != 0);
}

static inline PyObject* bool_obj(int8_t x) {
  return ISNA<int8_t>(x)? py::None().release() : py::obool(x).release();
}

template <typename T>
static inline PyObject* int_obj(T x) {
  return ISNA<T>(x)? py::None().release() : py::oint(x).release();
}

template <typename T>
static inline PyObject* real_obj(T x) {
  return ISNA<T>(x)? py::None().release() : py::ofloat(x).release();
}

static inline PyObject* obj_obj(PyObject* x) {
  return py::oobj(x).release();
}

template <typename T, size_t MAX = 30>
static inline void num_str(T x, dt::string_buf* buf) {
  char* ch = buf->prepare_raw_write(MAX);
  toa<T>(&ch, x);
  buf->commit_raw_write(ch);
}

static inline void bool_str(int8_t x, dt::string_buf* buf) {
  static CString str_true  {"True", 4};
  static CString str_false {"False", 5};
  buf->write(x? str_true : str_false);
}

static inline void obj_str(PyObject* x, dt::string_buf* buf) {
  CString xstr = py::robj(x).to_pystring_force().to_cstring();
  buf->write(xstr);
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
static void cast_to_pyobj(const Column* col, void* out_data)
{
  auto inp = static_cast<const T*>(col->data());
  auto out = static_cast<PyObject**>(out_data);
  const RowIndex& rowindex = col->rowindex();
  for (size_t i = 0; i < col->nrows; ++i) {
    Py_DECREF(out[i]);
    out[i] = CAST_OP(inp[rowindex[i]]);
  }
}


template <typename T>
static void cast_str_to_pyobj(const Column* col, void* out_data)
{
  auto scol = static_cast<const StringColumn<T>*>(col);
  auto offsets = scol->offsets();
  auto strdata = scol->strdata();
  auto out = static_cast<PyObject**>(out_data);
  const RowIndex& rowindex = col->rowindex();
  for (size_t i = 0; i < col->nrows; ++i) {
    Py_DECREF(out[i]);
    size_t j = rowindex[i];
    T off_start = offsets[j - 1] & ~GETNA<T>();
    T off_end = offsets[j];
    if (ISNA<T>(off_end)) {
      out[i] = py::None().release();
    } else {
      out[i] = py::ostring(strdata + off_start, off_end - off_start).release();
    }
  }
}



template <typename T, void (*CAST_OP)(T, dt::string_buf*)>
static Column* cast_to_str(const Column* col, MemoryRange&& out_offsets,
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
          CAST_OP(x, buf);
        }
      },
      col->nrows,
      std::move(out_offsets),
      /* force_str64 = */ (target_stype == SType::STR64),
      /* force_single_threaded = */ (col->stype() == SType::OBJ)
  );
}


template <typename T>
static Column* cast_str_to_str(const Column* col, MemoryRange&& out_offsets,
                               SType target_stype)
{
  auto scol = static_cast<const StringColumn<T>*>(col);
  auto offsets = scol->offsets();
  auto strdata = scol->strdata();
  const RowIndex& rowindex = scol->rowindex();
  if (sizeof(T) == 8 && target_stype == SType::STR32 &&
      (scol->datasize() > Column::MAX_STR32_BUFFER_SIZE ||
       scol->nrows > Column::MAX_STR32_NROWS)) {
    // If the user attempts to convert str64 into str32 but the column is too
    // big, we will convert into str64 instead.
    // We could have also thrown an exception here, but this seems to be more
    // in agreement with other cases where we silently promote str32->str64.
    return cast_str_to_str<T>(col, std::move(out_offsets), SType::STR64);
  }
  return dt::generate_string_column(
      [&](size_t i, dt::string_buf* buf) {
        size_t j = rowindex[i];
        T off_start = offsets[j - 1] & ~GETNA<T>();
        T off_end = offsets[j];
        if (ISNA<T>(off_end)) {
          buf->write_na();
        } else {
          buf->write(strdata + off_start, off_end - off_start);
        }
      },
      col->nrows,
      std::move(out_offsets),
      (target_stype == SType::STR64)
  );
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
  constexpr SType str32  = SType::STR32;
  constexpr SType str64  = SType::STR64;
  constexpr SType obj64  = SType::OBJ;

  // Trivial casts
  castfns0[id(bool8, bool8)]   = cast_fw0<int8_t,  int8_t,  _copy<int8_t>>;
  castfns0[id(int8, int8)]     = cast_fw0<int8_t,  int8_t,  _copy<int8_t>>;
  castfns0[id(int16, int16)]   = cast_fw0<int16_t, int16_t, _copy<int16_t>>;
  castfns0[id(int32, int32)]   = cast_fw0<int32_t, int32_t, _copy<int32_t>>;
  castfns0[id(int64, int64)]   = cast_fw0<int64_t, int64_t, _copy<int64_t>>;
  castfns0[id(real32, real32)] = cast_fw0<float,   float,   _copy<float>>;
  castfns0[id(real64, real64)] = cast_fw0<double,  double,  _copy<double>>;

  // Casts into bool8
  castfns0[id(int8, bool8)]   = cast_fw0<int8_t,  int8_t, fw_bool<int8_t>>;
  castfns0[id(int16, bool8)]  = cast_fw0<int16_t, int8_t, fw_bool<int16_t>>;
  castfns0[id(int32, bool8)]  = cast_fw0<int32_t, int8_t, fw_bool<int32_t>>;
  castfns0[id(int64, bool8)]  = cast_fw0<int64_t, int8_t, fw_bool<int64_t>>;
  castfns0[id(real32, bool8)] = cast_fw0<float,   int8_t, fw_bool<float>>;
  castfns0[id(real64, bool8)] = cast_fw0<double,  int8_t, fw_bool<double>>;

  // Casts into int8
  castfns0[id(bool8, int8)]   = cast_fw0<int8_t,  int8_t, fw_fw<int8_t, int8_t>>;
  castfns0[id(int16, int8)]   = cast_fw0<int16_t, int8_t, fw_fw<int16_t, int8_t>>;
  castfns0[id(int32, int8)]   = cast_fw0<int32_t, int8_t, fw_fw<int32_t, int8_t>>;
  castfns0[id(int64, int8)]   = cast_fw0<int64_t, int8_t, fw_fw<int64_t, int8_t>>;
  castfns0[id(real32, int8)]  = cast_fw0<float,   int8_t, fw_fw<float, int8_t>>;
  castfns0[id(real64, int8)]  = cast_fw0<double,  int8_t, fw_fw<double, int8_t>>;

  // Casts into int16
  castfns0[id(bool8, int16)]  = cast_fw0<int8_t,  int16_t, fw_fw<int8_t, int16_t>>;
  castfns0[id(int8, int16)]   = cast_fw0<int8_t,  int16_t, fw_fw<int8_t, int16_t>>;
  castfns0[id(int32, int16)]  = cast_fw0<int32_t, int16_t, fw_fw<int32_t, int16_t>>;
  castfns0[id(int64, int16)]  = cast_fw0<int64_t, int16_t, fw_fw<int64_t, int16_t>>;
  castfns0[id(real32, int16)] = cast_fw0<float,   int16_t, fw_fw<float, int16_t>>;
  castfns0[id(real64, int16)] = cast_fw0<double,  int16_t, fw_fw<double, int16_t>>;

  // Casts into int32
  castfns0[id(bool8, int32)]  = cast_fw0<int8_t,  int32_t, fw_fw<int8_t, int32_t>>;
  castfns0[id(int8, int32)]   = cast_fw0<int8_t,  int32_t, fw_fw<int8_t, int32_t>>;
  castfns0[id(int16, int32)]  = cast_fw0<int16_t, int32_t, fw_fw<int16_t, int32_t>>;
  castfns0[id(int64, int32)]  = cast_fw0<int64_t, int32_t, fw_fw<int64_t, int32_t>>;
  castfns0[id(real32, int32)] = cast_fw0<float,   int32_t, fw_fw<float, int32_t>>;
  castfns0[id(real64, int32)] = cast_fw0<double,  int32_t, fw_fw<double, int32_t>>;

  // Casts into int64
  castfns0[id(bool8, int64)]  = cast_fw0<int8_t,  int64_t, fw_fw<int8_t, int64_t>>;
  castfns0[id(int8, int64)]   = cast_fw0<int8_t,  int64_t, fw_fw<int8_t, int64_t>>;
  castfns0[id(int16, int64)]  = cast_fw0<int16_t, int64_t, fw_fw<int16_t, int64_t>>;
  castfns0[id(int32, int64)]  = cast_fw0<int32_t, int64_t, fw_fw<int32_t, int64_t>>;
  castfns0[id(real32, int64)] = cast_fw0<float,   int64_t, fw_fw<float, int64_t>>;
  castfns0[id(real64, int64)] = cast_fw0<double,  int64_t, fw_fw<double, int64_t>>;

  // Casts into real32
  castfns0[id(bool8, real32)]  = cast_fw0<int8_t,  float, fw_fw<int8_t, float>>;
  castfns0[id(int8, real32)]   = cast_fw0<int8_t,  float, fw_fw<int8_t, float>>;
  castfns0[id(int16, real32)]  = cast_fw0<int16_t, float, fw_fw<int16_t, float>>;
  castfns0[id(int32, real32)]  = cast_fw0<int32_t, float, fw_fw<int32_t, float>>;
  castfns0[id(int64, real32)]  = cast_fw0<int64_t, float, fw_fw<int64_t, float>>;
  castfns0[id(real64, real32)] = cast_fw0<double,  float, _static<double, float>>;

  // Casts into real64
  castfns0[id(bool8, real64)]  = cast_fw0<int8_t,  double, fw_fw<int8_t, double>>;
  castfns0[id(int8, real64)]   = cast_fw0<int8_t,  double, fw_fw<int8_t, double>>;
  castfns0[id(int16, real64)]  = cast_fw0<int16_t, double, fw_fw<int16_t, double>>;
  castfns0[id(int32, real64)]  = cast_fw0<int32_t, double, fw_fw<int32_t, double>>;
  castfns0[id(int64, real64)]  = cast_fw0<int64_t, double, fw_fw<int64_t, double>>;
  castfns0[id(real32, real64)] = cast_fw0<float,   double, _static<float, double>>;

  // Casts into str32
  castfnsx[id(bool8, str32)]  = cast_to_str<int8_t, bool_str>;
  castfnsx[id(int8, str32)]   = cast_to_str<int8_t, num_str<int8_t>>;
  castfnsx[id(int16, str32)]  = cast_to_str<int16_t, num_str<int16_t>>;
  castfnsx[id(int32, str32)]  = cast_to_str<int32_t, num_str<int32_t>>;
  castfnsx[id(int64, str32)]  = cast_to_str<int64_t, num_str<int64_t>>;
  castfnsx[id(real32, str32)] = cast_to_str<float, num_str<float>>;
  castfnsx[id(real64, str32)] = cast_to_str<double, num_str<double>>;
  castfnsx[id(str32, str32)]  = cast_str_to_str<uint32_t>;
  castfnsx[id(str64, str32)]  = cast_str_to_str<uint64_t>;
  castfnsx[id(obj64, str32)]  = cast_to_str<PyObject*, obj_str>;

  // Casts into str64
  castfnsx[id(bool8, str64)]  = cast_to_str<int8_t, bool_str>;
  castfnsx[id(int8, str64)]   = cast_to_str<int8_t, num_str<int8_t>>;
  castfnsx[id(int16, str64)]  = cast_to_str<int16_t, num_str<int16_t>>;
  castfnsx[id(int32, str64)]  = cast_to_str<int32_t, num_str<int32_t>>;
  castfnsx[id(int64, str64)]  = cast_to_str<int64_t, num_str<int64_t>>;
  castfnsx[id(real32, str64)] = cast_to_str<float, num_str<float>>;
  castfnsx[id(real64, str64)] = cast_to_str<double, num_str<double>>;
  castfnsx[id(str32, str64)]  = cast_str_to_str<uint32_t>;
  castfnsx[id(str64, str64)]  = cast_str_to_str<uint64_t>;
  castfnsx[id(obj64, str64)]  = cast_to_str<PyObject*, obj_str>;

  // Casts into obj64
  castfns2[id(bool8, obj64)]  = cast_to_pyobj<int8_t,    bool_obj>;
  castfns2[id(int8, obj64)]   = cast_to_pyobj<int8_t,    int_obj<int8_t>>;
  castfns2[id(int16, obj64)]  = cast_to_pyobj<int16_t,   int_obj<int16_t>>;
  castfns2[id(int32, obj64)]  = cast_to_pyobj<int32_t,   int_obj<int32_t>>;
  castfns2[id(int64, obj64)]  = cast_to_pyobj<int64_t,   int_obj<int64_t>>;
  castfns2[id(real32, obj64)] = cast_to_pyobj<float,     real_obj<float>>;
  castfns2[id(real64, obj64)] = cast_to_pyobj<double,    real_obj<double>>;
  castfns2[id(str32, obj64)]  = cast_str_to_pyobj<uint32_t>;
  castfns2[id(str64, obj64)]  = cast_str_to_pyobj<uint64_t>;
  castfns2[id(obj64, obj64)]  = cast_to_pyobj<PyObject*, obj_obj>;
}
