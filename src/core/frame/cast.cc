//------------------------------------------------------------------------------
// Copyright 2018-2019 H2O.ai
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
#include "parallel/api.h"           // dt::parallel_for_static
#include "parallel/string_utils.h"  // dt::generate_string_column
#include "python/_all.h"
#include "python/string.h"
#include "column.h"
#include "datatablemodule.h"


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
  return py::obool(x).release();
}

template <typename T>
static inline PyObject* int_obj(T x) {
  return py::oint(x).release();
}

template <typename T>
static inline PyObject* real_obj(T x) {
  return py::ofloat(x).release();
}

static inline PyObject* str_obj(CString x) {
  return py::ostring(x).release();
}

static inline PyObject* obj_obj(py::robj x) {
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

static inline void obj_str(py::robj x, dt::string_buf* buf) {
  CString xstr = x.to_pystring_force().to_cstring();
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
static void cast_fw0(const Column& col, size_t start, void* out_data)
{
  auto inp = static_cast<const T*>(col.get_data_readonly()) + start;
  auto out = static_cast<U*>(out_data);
  dt::parallel_for_static(col.nrows(),
    dt::NThreads(col.allow_parallel_access()),
    [=](size_t i) {
      out[i] = CAST_OP(inp[i]);
    });
}


template <typename T, typename U, U(*CAST_OP)(T)>
static void cast_fw2(const Column& col, void* out_data)
{
  auto out = static_cast<U*>(out_data);
  dt::parallel_for_static(col.nrows(),
    dt::NThreads(col.allow_parallel_access()),
    [=](size_t i) {
      T value;
      bool isvalid = col.get_element(i, &value);
      out[i] = isvalid? CAST_OP(value) : GETNA<U>();
    });
}


// Casting into PyObject* can only be done in single-threaded mode.
// Note that when casting into PyObject buffer, we assume that it
// is safe to simply overwrite the contents of that buffer. Thus,
// the buffer should not contain any existing PyObjects.
//
template <typename T, PyObject* (*CAST_OP)(T)>
static void cast_to_pyobj(const Column& col, void* out_data)
{
  auto out = static_cast<PyObject**>(out_data);
  T value;
  for (size_t i = 0; i < col.nrows(); ++i) {
    bool isvalid = col.get_element(i, &value);
    out[i] = isvalid? CAST_OP(value) : py::None().release();
  }
}



template <typename T, void (*CAST_OP)(T, dt::string_buf*)>
static Column cast_to_str(const Column& col, Buffer&& out_offsets,
                          SType target_stype)
{
  return dt::generate_string_column(
      [&](size_t i, dt::string_buf* buf) {
        T value;
        bool isvalid = col.get_element(i, &value);
        if (isvalid) {
          CAST_OP(value, buf);
        } else {
          buf->write_na();
        }
      },
      col.nrows(),
      std::move(out_offsets),
      /* force_str64 = */ (target_stype == SType::STR64),
      /* force_single_threaded = */ !col.allow_parallel_access()
  );
}


template <typename T>
static Column cast_str_to_str(const Column& col, Buffer&& out_offsets,
                              SType target_stype)
{
  if (sizeof(T) == 8 && target_stype == SType::STR32 &&
      ( // col->datasize() > Column::MAX_ARR32_SIZE ||
       col.nrows() > Column::MAX_ARR32_SIZE)) {
    // If the user attempts to convert str64 into str32 but the column is too
    // big, we will convert into str64 instead.
    // We could have also thrown an exception here, but this seems to be more
    // in agreement with other cases where we silently promote str32->str64.
    return cast_str_to_str<T>(col, std::move(out_offsets), SType::STR64);
  }
  return dt::generate_string_column(
      [&](size_t i, dt::string_buf* buf) {
        CString value;
        bool isvalid = col.get_element(i, &value);
        if (isvalid) {
          buf->write(value);
        } else {
          buf->write_na();
        }
      },
      col.nrows(),
      std::move(out_offsets),
      /* force_str64 = */ (target_stype == SType::STR64),
      /* force_single_threaded = */ !col.allow_parallel_access()
  );
}




//------------------------------------------------------------------------------
// cast_manager
//------------------------------------------------------------------------------

class cast_manager {
  private:
    using castfn0 = void (*)(const Column&, size_t start, void* out);
    using castfn2 = void (*)(const Column&, void* out);
    using castfnx = Column (*)(const Column&, Buffer&&, SType);
    struct cast_info {
      castfn0  f0;
      castfn2  f2;
      castfnx  fx;
      cast_info() : f0(nullptr), f2(nullptr), fx(nullptr) {}
    };
    std::unordered_map<size_t, cast_info> all_casts;

  public:

    inline void add(SType st_from, SType st_to, castfn0 f);
    inline void add(SType st_from, SType st_to, castfn2 f);
    inline void add(SType st_from, SType st_to, castfnx f);

    Column execute(const Column&, Buffer&&, SType);

  private:
    static inline constexpr size_t key(SType st1, SType st2) {
      return static_cast<size_t>(st1) * DT_STYPES_COUNT +
             static_cast<size_t>(st2);
    }
};


void cast_manager::add(SType st_from, SType st_to, castfn0 f) {
  size_t id = key(st_from, st_to);
  xassert(!all_casts[id].f0);
  all_casts[id].f0 = f;
}

void cast_manager::add(SType st_from, SType st_to, castfn2 f) {
  size_t id = key(st_from, st_to);
  xassert(!all_casts[id].f2);
  all_casts[id].f2 = f;
}

void cast_manager::add(SType st_from, SType st_to, castfnx f) {
  size_t id = key(st_from, st_to);
  xassert(!all_casts[id].fx);
  all_casts[id].fx = f;
}


Column cast_manager::execute(const Column& src, Buffer&& target_mbuf,
                             SType target_stype)
{
  xassert(!target_mbuf.is_pyobjects());
  size_t nrows = src.nrows();
  if (src.stype() == SType::VOID) {
    return Column::new_na_column(nrows, target_stype);
  }

  size_t id = key(src.stype(), target_stype);
  if (all_casts.count(id) == 0) {
    throw NotImplError()
        << "Unable to cast `" << src.stype() << "` into `"
        << target_stype << "`";
  }

  const auto& castfns = all_casts[id];
  if (castfns.fx) {
    return castfns.fx(src, std::move(target_mbuf), target_stype);
  }
  xassert(castfns.f2);

  target_mbuf.resize(nrows * info(target_stype).elemsize());
  void* out_data = target_mbuf.wptr();

  if (src.is_virtual() || !castfns.f0) {
    castfns.f2(src, out_data);
  }
  else {
    castfns.f0(src, 0, out_data);
  }

  if (target_stype == SType::OBJ) {
    target_mbuf.set_pyobjects(/* clear = */ false);
  }

  return Column::new_mbuf_column(nrows, target_stype, std::move(target_mbuf));
}




//------------------------------------------------------------------------------
// One-time initialization
//------------------------------------------------------------------------------

static cast_manager casts;


void py::DatatableModule::init_casts()
{
  // cast_fw0: cast a fw column without rowindex
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
  casts.add(bool8, bool8,   cast_fw0<int8_t,  int8_t,  _copy<int8_t>>);
  casts.add(int8, int8,     cast_fw0<int8_t,  int8_t,  _copy<int8_t>>);
  casts.add(int16, int16,   cast_fw0<int16_t, int16_t, _copy<int16_t>>);
  casts.add(int32, int32,   cast_fw0<int32_t, int32_t, _copy<int32_t>>);
  casts.add(int64, int64,   cast_fw0<int64_t, int64_t, _copy<int64_t>>);
  casts.add(real32, real32, cast_fw0<float,   float,   _copy<float>>);
  casts.add(real64, real64, cast_fw0<double,  double,  _copy<double>>);

  casts.add(bool8, bool8,   cast_fw2<int8_t,  int8_t,  _copy<int8_t>>);
  casts.add(int8, int8,     cast_fw2<int8_t,  int8_t,  _copy<int8_t>>);
  casts.add(int16, int16,   cast_fw2<int16_t, int16_t, _copy<int16_t>>);
  casts.add(int32, int32,   cast_fw2<int32_t, int32_t, _copy<int32_t>>);
  casts.add(int64, int64,   cast_fw2<int64_t, int64_t, _copy<int64_t>>);
  casts.add(real32, real32, cast_fw2<float,   float,   _copy<float>>);
  casts.add(real64, real64, cast_fw2<double,  double,  _copy<double>>);

  // Casts into bool8
  casts.add(int8, bool8,   cast_fw2<int8_t,  int8_t, fw_bool<int8_t>>);
  casts.add(int16, bool8,  cast_fw2<int16_t, int8_t, fw_bool<int16_t>>);
  casts.add(int32, bool8,  cast_fw2<int32_t, int8_t, fw_bool<int32_t>>);
  casts.add(int64, bool8,  cast_fw2<int64_t, int8_t, fw_bool<int64_t>>);
  casts.add(real32, bool8, cast_fw2<float,   int8_t, fw_bool<float>>);
  casts.add(real64, bool8, cast_fw2<double,  int8_t, fw_bool<double>>);

  // Casts into int8
  casts.add(bool8, int8,   cast_fw2<int8_t,  int8_t, fw_fw<int8_t, int8_t>>);
  casts.add(int16, int8,   cast_fw2<int16_t, int8_t, fw_fw<int16_t, int8_t>>);
  casts.add(int32, int8,   cast_fw2<int32_t, int8_t, fw_fw<int32_t, int8_t>>);
  casts.add(int64, int8,   cast_fw2<int64_t, int8_t, fw_fw<int64_t, int8_t>>);
  casts.add(real32, int8,  cast_fw2<float,   int8_t, fw_fw<float, int8_t>>);
  casts.add(real64, int8,  cast_fw2<double,  int8_t, fw_fw<double, int8_t>>);

  // Casts into int16
  casts.add(bool8, int16,  cast_fw2<int8_t,  int16_t, fw_fw<int8_t, int16_t>>);
  casts.add(int8, int16,   cast_fw2<int8_t,  int16_t, fw_fw<int8_t, int16_t>>);
  casts.add(int32, int16,  cast_fw2<int32_t, int16_t, fw_fw<int32_t, int16_t>>);
  casts.add(int64, int16,  cast_fw2<int64_t, int16_t, fw_fw<int64_t, int16_t>>);
  casts.add(real32, int16, cast_fw2<float,   int16_t, fw_fw<float, int16_t>>);
  casts.add(real64, int16, cast_fw2<double,  int16_t, fw_fw<double, int16_t>>);

  // Casts into int32
  casts.add(bool8, int32,  cast_fw0<int8_t,  int32_t, fw_fw<int8_t, int32_t>>);
  casts.add(int8, int32,   cast_fw0<int8_t,  int32_t, fw_fw<int8_t, int32_t>>);
  casts.add(int16, int32,  cast_fw0<int16_t, int32_t, fw_fw<int16_t, int32_t>>);
  casts.add(int64, int32,  cast_fw0<int64_t, int32_t, fw_fw<int64_t, int32_t>>);
  casts.add(real32, int32, cast_fw0<float,   int32_t, fw_fw<float, int32_t>>);
  casts.add(real64, int32, cast_fw0<double,  int32_t, fw_fw<double, int32_t>>);

  casts.add(bool8, int32,  cast_fw2<int8_t,  int32_t, fw_fw<int8_t, int32_t>>);
  casts.add(int8, int32,   cast_fw2<int8_t,  int32_t, fw_fw<int8_t, int32_t>>);
  casts.add(int16, int32,  cast_fw2<int16_t, int32_t, fw_fw<int16_t, int32_t>>);
  casts.add(int64, int32,  cast_fw2<int64_t, int32_t, fw_fw<int64_t, int32_t>>);
  casts.add(real32, int32, cast_fw2<float,   int32_t, fw_fw<float, int32_t>>);
  casts.add(real64, int32, cast_fw2<double,  int32_t, fw_fw<double, int32_t>>);

  // Casts into int64
  casts.add(bool8, int64,  cast_fw0<int8_t,  int64_t, fw_fw<int8_t, int64_t>>);
  casts.add(int8, int64,   cast_fw0<int8_t,  int64_t, fw_fw<int8_t, int64_t>>);
  casts.add(int16, int64,  cast_fw0<int16_t, int64_t, fw_fw<int16_t, int64_t>>);
  casts.add(int32, int64,  cast_fw0<int32_t, int64_t, fw_fw<int32_t, int64_t>>);
  casts.add(real32, int64, cast_fw0<float,   int64_t, fw_fw<float, int64_t>>);
  casts.add(real64, int64, cast_fw0<double,  int64_t, fw_fw<double, int64_t>>);

  casts.add(bool8, int64,  cast_fw2<int8_t,  int64_t, fw_fw<int8_t, int64_t>>);
  casts.add(int8, int64,   cast_fw2<int8_t,  int64_t, fw_fw<int8_t, int64_t>>);
  casts.add(int16, int64,  cast_fw2<int16_t, int64_t, fw_fw<int16_t, int64_t>>);
  casts.add(int32, int64,  cast_fw2<int32_t, int64_t, fw_fw<int32_t, int64_t>>);
  casts.add(real32, int64, cast_fw2<float,   int64_t, fw_fw<float, int64_t>>);
  casts.add(real64, int64, cast_fw2<double,  int64_t, fw_fw<double, int64_t>>);

  // Casts into real32
  casts.add(bool8, real32,  cast_fw0<int8_t,  float, fw_fw<int8_t, float>>);
  casts.add(int8, real32,   cast_fw0<int8_t,  float, fw_fw<int8_t, float>>);
  casts.add(int16, real32,  cast_fw0<int16_t, float, fw_fw<int16_t, float>>);
  casts.add(int32, real32,  cast_fw0<int32_t, float, fw_fw<int32_t, float>>);
  casts.add(int64, real32,  cast_fw0<int64_t, float, fw_fw<int64_t, float>>);
  casts.add(real64, real32, cast_fw0<double,  float, _static<double, float>>);

  casts.add(bool8, real32,  cast_fw2<int8_t,  float, fw_fw<int8_t, float>>);
  casts.add(int8, real32,   cast_fw2<int8_t,  float, fw_fw<int8_t, float>>);
  casts.add(int16, real32,  cast_fw2<int16_t, float, fw_fw<int16_t, float>>);
  casts.add(int32, real32,  cast_fw2<int32_t, float, fw_fw<int32_t, float>>);
  casts.add(int64, real32,  cast_fw2<int64_t, float, fw_fw<int64_t, float>>);
  casts.add(real64, real32, cast_fw2<double,  float, _static<double, float>>);

  // Casts into real64
  casts.add(bool8, real64,  cast_fw0<int8_t,  double, fw_fw<int8_t, double>>);
  casts.add(int8, real64,   cast_fw0<int8_t,  double, fw_fw<int8_t, double>>);
  casts.add(int16, real64,  cast_fw0<int16_t, double, fw_fw<int16_t, double>>);
  casts.add(int32, real64,  cast_fw0<int32_t, double, fw_fw<int32_t, double>>);
  casts.add(int64, real64,  cast_fw0<int64_t, double, fw_fw<int64_t, double>>);
  casts.add(real32, real64, cast_fw0<float,   double, _static<float, double>>);

  casts.add(bool8, real64,  cast_fw2<int8_t,  double, fw_fw<int8_t, double>>);
  casts.add(int8, real64,   cast_fw2<int8_t,  double, fw_fw<int8_t, double>>);
  casts.add(int16, real64,  cast_fw2<int16_t, double, fw_fw<int16_t, double>>);
  casts.add(int32, real64,  cast_fw2<int32_t, double, fw_fw<int32_t, double>>);
  casts.add(int64, real64,  cast_fw2<int64_t, double, fw_fw<int64_t, double>>);
  casts.add(real32, real64, cast_fw2<float,   double, _static<float, double>>);

  // Casts into str32
  casts.add(bool8, str32,  cast_to_str<int8_t, bool_str>);
  casts.add(int8, str32,   cast_to_str<int8_t, num_str<int8_t>>);
  casts.add(int16, str32,  cast_to_str<int16_t, num_str<int16_t>>);
  casts.add(int32, str32,  cast_to_str<int32_t, num_str<int32_t>>);
  casts.add(int64, str32,  cast_to_str<int64_t, num_str<int64_t>>);
  casts.add(real32, str32, cast_to_str<float, num_str<float>>);
  casts.add(real64, str32, cast_to_str<double, num_str<double>>);
  casts.add(str32, str32,  cast_str_to_str<uint32_t>);
  casts.add(str64, str32,  cast_str_to_str<uint64_t>);
  casts.add(obj64, str32,  cast_to_str<py::robj, obj_str>);

  // Casts into str64
  casts.add(bool8, str64,  cast_to_str<int8_t, bool_str>);
  casts.add(int8, str64,   cast_to_str<int8_t, num_str<int8_t>>);
  casts.add(int16, str64,  cast_to_str<int16_t, num_str<int16_t>>);
  casts.add(int32, str64,  cast_to_str<int32_t, num_str<int32_t>>);
  casts.add(int64, str64,  cast_to_str<int64_t, num_str<int64_t>>);
  casts.add(real32, str64, cast_to_str<float, num_str<float>>);
  casts.add(real64, str64, cast_to_str<double, num_str<double>>);
  casts.add(str32, str64,  cast_str_to_str<uint32_t>);
  casts.add(str64, str64,  cast_str_to_str<uint64_t>);
  casts.add(obj64, str64,  cast_to_str<py::robj, obj_str>);

  // Casts into obj64
  casts.add(bool8, obj64,  cast_to_pyobj<int8_t,   bool_obj>);
  casts.add(int8, obj64,   cast_to_pyobj<int8_t,   int_obj<int8_t>>);
  casts.add(int16, obj64,  cast_to_pyobj<int16_t,  int_obj<int16_t>>);
  casts.add(int32, obj64,  cast_to_pyobj<int32_t,  int_obj<int32_t>>);
  casts.add(int64, obj64,  cast_to_pyobj<int64_t,  int_obj<int64_t>>);
  casts.add(real32, obj64, cast_to_pyobj<float,    real_obj<float>>);
  casts.add(real64, obj64, cast_to_pyobj<double,   real_obj<double>>);
  casts.add(str32, obj64,  cast_to_pyobj<CString,  str_obj>);
  casts.add(str64, obj64,  cast_to_pyobj<CString,  str_obj>);
  casts.add(obj64, obj64,  cast_to_pyobj<py::robj, obj_obj>);
}




//------------------------------------------------------------------------------
// Column (base methods)
//------------------------------------------------------------------------------

void Column::cast_inplace(SType stype) {
  Column newcolumn = casts.execute(*this, Buffer(), stype);
  std::swap(*this, newcolumn);
}


Column Column::cast(SType stype) const {
  return casts.execute(*this, Buffer(), stype);
}


Column Column::cast(SType stype, Buffer&& mem) const {
  return casts.execute(*this, std::move(mem), stype);
}
