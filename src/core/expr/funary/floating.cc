//------------------------------------------------------------------------------
// Copyright 2019 H2O.ai
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
#include <cmath>
#include "expr/funary/pyfn.h"
#include "expr/funary/umaker.h"
#include "expr/funary/umaker_impl.h"
namespace dt {
namespace expr {



//------------------------------------------------------------------------------
// Op::ABS
//------------------------------------------------------------------------------

static const char* doc_abs =
R"(abs(x)
--

The absolute value of x.

The argument x must have one of the numeric types: boolean, integer
or float. If x has stype bool, int8 or int16 then the return value
is promoted to int32, otherwise the stype of the result is the same
as the stype of the argument.
)";

py::PKArgs args_abs(1, 0, 0, false, false, {"x"}, "abs", doc_abs);

template <typename T>
static T op_abs(T x) {
  return (x >= 0)? x : -x;
}

template <typename T>
static inline umaker_ptr _abs(SType uptype = SType::VOID) {
  return umaker1<T, T>::make(op_abs<T>, uptype, stype_from<T>());
}

umaker_ptr resolve_op_abs(SType stype) {
  switch (stype) {
    case SType::VOID:    return umaker_ptr(new umaker_nacol());
    case SType::BOOL:    return umaker_ptr(new umaker_cast(SType::INT32));
    case SType::INT8:
    case SType::INT16:   return _abs<int32_t>(/* uptype= */ SType::INT32);
    case SType::INT32:   return _abs<int32_t>();
    case SType::INT64:   return _abs<int64_t>();
    case SType::FLOAT32: return _abs<float>();
    case SType::FLOAT64: return _abs<double>();
    default:
      throw TypeError() << "Function `abs` cannot be applied to a "
                           "column of type `" << stype << "`";
  }
}




//------------------------------------------------------------------------------
// Op::FABS
//------------------------------------------------------------------------------

static const char* doc_fabs =
R"(fabs(x)
--

The absolute value of x, returned as float.
)";

py::PKArgs args_fabs(1, 0, 0, false, false, {"x"}, "fabs", doc_fabs);


umaker_ptr resolve_op_fabs(SType stype) {
  switch (stype) {
    case SType::VOID:    return umaker_ptr(new umaker_nacol());
    case SType::BOOL:    return umaker_ptr(new umaker_cast(SType::FLOAT64));
    case SType::INT8:
    case SType::INT16:
    case SType::INT32:
    case SType::INT64:   return _abs<double>(/* uptype= */ SType::FLOAT64);
    case SType::FLOAT32: return _abs<float>();
    case SType::FLOAT64: return _abs<double>();
    default:
      throw TypeError() << "Function `fabs` cannot be applied to a "
                           "column of type `" << stype << "`";
  }
}




//------------------------------------------------------------------------------
// Op::SIGN
//------------------------------------------------------------------------------

static const char* doc_sign =
R"(sign(x)
--

The sign of x, returned as float.

This function returns 1.0 if x is positive (including positive
infinity), -1.0 if x is negative, 0.0 if x is zero, and NA if
x is NA.
)";

py::PKArgs args_sign(1, 0, 0, false, false, {"x"}, "sign", doc_sign);


template <typename T>
static T op_sign(T x) {
  return (x > 0)? T(1) : (x < 0)? T(-1) : T(0);
}

template <typename T>
static inline umaker_ptr _sign(SType uptype = SType::VOID) {
  return umaker1<T, T>::make(op_sign<T>, uptype, stype_from<T>());
}

umaker_ptr resolve_op_sign(SType stype) {
  switch (stype) {
    case SType::VOID:    return umaker_ptr(new umaker_nacol());
    case SType::BOOL:    return umaker_ptr(new umaker_cast(SType::FLOAT64));
    case SType::INT8:
    case SType::INT16:
    case SType::INT32:
    case SType::INT64:   return _sign<double>(SType::FLOAT64);
    case SType::FLOAT32: return _sign<float>();
    case SType::FLOAT64: return _sign<double>();
    default:
      throw TypeError() << "Function `sign` cannot be applied to a "
                           "column of type `" << stype << "`";
  }
}




//------------------------------------------------------------------------------
// Op::ISNA
//------------------------------------------------------------------------------

static const char* doc_isna =
R"(isna(x)
--

Returns True if the argument is NA, and False otherwise.
)";

py::PKArgs args_isna(1, 0, 0, false, false, {"x"}, "isna", doc_isna);


template <typename T>
class Isna_ColumnImpl : public Virtual_ColumnImpl {
  protected:
    Column arg_;

  public:
    Isna_ColumnImpl(Column&& col, size_t nrows)
      : Virtual_ColumnImpl(nrows, SType::BOOL),
        arg_(std::move(col)) { assert_compatible_type<T>(arg_.stype()); }

    ColumnImpl* clone() const override {
      return new Isna_ColumnImpl<T>(Column(arg_), nrows_);
    }

    bool allow_parallel_access() const override {
      return arg_.allow_parallel_access();
    }

    bool get_element(size_t i, int8_t* out) const override {
      T tmp;
      *out = ! arg_.get_element(i, &tmp);
      return true;
    }
};

template <typename T>
class isna_umaker : public umaker {
  public:
    Column compute(Column&& col) const override {
      size_t nrows = col.nrows();
      return Column(new Isna_ColumnImpl<T>(std::move(col), nrows));
    }
};


umaker_ptr resolve_op_isna(SType stype) {
  switch (stype) {
    case SType::VOID: {
      return umaker_ptr(new umaker_const(
                            Const_ColumnImpl::make_bool_column(1, true)));
    }
    case SType::BOOL:
    case SType::INT8:    return umaker_ptr(new isna_umaker<int8_t>());
    case SType::INT16:   return umaker_ptr(new isna_umaker<int16_t>());
    case SType::INT32:   return umaker_ptr(new isna_umaker<int32_t>());
    case SType::INT64:   return umaker_ptr(new isna_umaker<int64_t>());
    case SType::FLOAT32: return umaker_ptr(new isna_umaker<float>());
    case SType::FLOAT64: return umaker_ptr(new isna_umaker<double>());
    case SType::STR32:
    case SType::STR64:   return umaker_ptr(new isna_umaker<CString>());
    default:
      throw TypeError() << "Function `isna` cannot be applied to a "
                           "column of type `" << stype << "`";
  }
}




//------------------------------------------------------------------------------
// Op::ISINF
//------------------------------------------------------------------------------

static const char* doc_isinf =
R"(isinf(x)
--

Returns True if the argument is +/- infinity, and False otherwise.
Note that `isinf(NA) == False`.
)";

py::PKArgs args_isinf(1, 0, 0, false, false, {"x"}, "isinf", doc_isinf);


template <typename T>
static bool op_isinf(T x, bool isvalid, int8_t* out) {
  *out = isvalid && std::isinf(x);
  return true;
}

template <typename T>
static inline umaker_ptr _isinf() {
  return umaker2<T, int8_t>::make(op_isinf<T>, SType::VOID, SType::BOOL);
}

umaker_ptr resolve_op_isinf(SType stype) {
  switch (stype) {
    case SType::VOID:
    case SType::BOOL:
    case SType::INT8:
    case SType::INT16:
    case SType::INT32:
    case SType::INT64: {
      return umaker_ptr(new umaker_const(
                            Const_ColumnImpl::make_bool_column(1, false)));
    }
    case SType::FLOAT32: return _isinf<float>();
    case SType::FLOAT64: return _isinf<double>();
    default:
      throw TypeError() << "Function `isinf` cannot be applied to a "
                           "column of type `" << stype << "`";
  }
}




//------------------------------------------------------------------------------
// Op::ISFINITE
//------------------------------------------------------------------------------

static const char* doc_isfinite =
R"(isfinite(x)
--

Returns True if x has a finite value, and False if x is infinity
or NaN. This function is equivalent to ``!(isna(x) or isinf(x))``.
)";

py::PKArgs args_isfinite(1, 0, 0, false, false, {"x"}, "isfinite", doc_isfinite);


template <typename T>
static bool op_isfinite(T x, bool xvalid, int8_t* out) {
  *out = xvalid && std::isfinite(x);
  return true;
}

template <typename T>
static bool op_notna(T, bool xvalid, int8_t* out) {
  *out = xvalid;
  return true;
}


template <typename T>
static umaker_ptr _isfinite_int(SType uptype = SType::VOID) {
  return umaker2<T, int8_t>::make(op_notna<T>, uptype, SType::BOOL);
}

template <typename T>
static umaker_ptr _isfinite_float() {
  return umaker2<T, int8_t>::make(op_isfinite<T>, SType::VOID, SType::BOOL);
}


umaker_ptr resolve_op_isfinite(SType stype) {
  switch (stype) {
    case SType::VOID: {
      return umaker_ptr(new umaker_const(
                            Const_ColumnImpl::make_bool_column(1, false)));
    }
    case SType::BOOL:
    case SType::INT8:
    case SType::INT16:   return _isfinite_int<int32_t>(SType::INT32);
    case SType::INT32:   return _isfinite_int<int32_t>();
    case SType::INT64:   return _isfinite_int<int64_t>();
    case SType::FLOAT32: return _isfinite_float<float>();
    case SType::FLOAT64: return _isfinite_float<double>();
    default:
      throw TypeError() << "Function `isfinite` cannot be applied to a "
                           "column of type `" << stype << "`";
  }
}




//------------------------------------------------------------------------------
// Op::CEIL
//------------------------------------------------------------------------------

static const char* doc_ceil =
R"(ceil(x)
--

The smallest integer value not less than `x`, returned as float.
)";

py::PKArgs args_ceil(1, 0, 0, false, false, {"x"}, "ceil", doc_ceil);


template <typename T>
static umaker_ptr _ceil() {
  return umaker1<T, T>::make(std::ceil, SType::VOID, stype_from<T>());
}

umaker_ptr resolve_op_ceil(SType stype) {
  switch (stype) {
    case SType::VOID:    return umaker_ptr(new umaker_nacol());
    case SType::BOOL:
    case SType::INT8:
    case SType::INT16:
    case SType::INT32:
    case SType::INT64:   return umaker_ptr(new umaker_cast(SType::FLOAT64));
    case SType::FLOAT32: return _ceil<float>();
    case SType::FLOAT64: return _ceil<double>();
    default:
      throw TypeError() << "Function `ceil` cannot be applied to a "
                           "column of type `" << stype << "`";
  }
}




//------------------------------------------------------------------------------
// Op::FLOOR
//------------------------------------------------------------------------------

static const char* doc_floor =
R"(floor(x)
--

The largest integer value not greater than `x`, returned as float.
)";

py::PKArgs args_floor(1, 0, 0, false, false, {"x"}, "floor", doc_floor);


template <typename T>
static umaker_ptr _floor() {
  return umaker1<T, T>::make(std::floor, SType::VOID, stype_from<T>());
}

umaker_ptr resolve_op_floor(SType stype) {
  switch (stype) {
    case SType::VOID:    return umaker_ptr(new umaker_nacol());
    case SType::BOOL:
    case SType::INT8:
    case SType::INT16:
    case SType::INT32:
    case SType::INT64:   return umaker_ptr(new umaker_cast(SType::FLOAT64));
    case SType::FLOAT32: return _floor<float>();
    case SType::FLOAT64: return _floor<double>();
    default:
      throw TypeError() << "Function `floor` cannot be applied to a "
                           "column of type `" << stype << "`";
  }
}




//------------------------------------------------------------------------------
// Op::RINT
//------------------------------------------------------------------------------

static const char* doc_rint =
R"(rint(x)
--

Round the value `x` to the nearest integer.
)";

py::PKArgs args_rint(1, 0, 0, false, false, {"x"}, "rint", doc_rint);


template <typename T>
static umaker_ptr _rint() {
  return umaker1<T, T>::make(std::rint, SType::VOID, stype_from<T>());
}

umaker_ptr resolve_op_rint(SType stype) {
  switch (stype) {
    case SType::VOID:    return umaker_ptr(new umaker_nacol());
    case SType::BOOL:
    case SType::INT8:
    case SType::INT16:
    case SType::INT32:
    case SType::INT64:   return umaker_ptr(new umaker_cast(SType::FLOAT64));
    case SType::FLOAT32: return _rint<float>();
    case SType::FLOAT64: return _rint<double>();
    default:
      throw TypeError() << "Function `rint` cannot be applied to a "
                           "column of type `" << stype << "`";
  }
}




//------------------------------------------------------------------------------
// Op::TRUNC
//------------------------------------------------------------------------------

static const char* doc_trunc =
R"(trunc(x)
--

The nearest integer value not greater than `x` in magnitude.

If x is integer or boolean, then trunc() will return this value
converted to float64. If x is floating-point, then trunc(x) acts as
floor(x) for positive values of x, and as ceil(x) for negative values
of x. This rounding mode is known as rounding towards zero.
)";

py::PKArgs args_trunc(1, 0, 0, false, false, {"x"}, "trunc", doc_trunc);


template <typename T>
static umaker_ptr _trunc() {
  return umaker1<T, T>::make(std::trunc, SType::VOID, stype_from<T>());
}

umaker_ptr resolve_op_trunc(SType stype) {
  switch (stype) {
    case SType::VOID:    return umaker_ptr(new umaker_nacol());
    case SType::BOOL:
    case SType::INT8:
    case SType::INT16:
    case SType::INT32:
    case SType::INT64:   return umaker_ptr(new umaker_cast(SType::FLOAT64));
    case SType::FLOAT32: return _trunc<float>();
    case SType::FLOAT64: return _trunc<double>();
    default:
      throw TypeError() << "Function `trunc` cannot be applied to a "
                           "column of type `" << stype << "`";
  }
}




//------------------------------------------------------------------------------
// Op::SIGNBIT
//------------------------------------------------------------------------------

static const char* doc_signbit =
R"(signbit(x)
--

Returns True if x is negative (its sign bit is set), and False if
x is positive. This function is able to distinguish between -0.0 and
+0.0, returning True/False respectively. If x is an NA value, this
function will also return NA.
)";

py::PKArgs args_signbit(1, 0, 0, false, false, {"x"}, "signbit", doc_signbit);


template <typename T>
static int8_t op_signbit(T x) { return std::signbit(x); }

template <typename T>
static umaker_ptr _signbit(SType uptype = SType::VOID) {
  return umaker1<T, int8_t>::make(op_signbit<T>, uptype, SType::BOOL);
}

umaker_ptr resolve_op_signbit(SType stype) {
  switch (stype) {
    case SType::VOID:    return umaker_ptr(new umaker_nacol());
    case SType::BOOL:
    case SType::INT8:
    case SType::INT16:
    case SType::INT32:
    case SType::INT64:   return _signbit<double>(SType::FLOAT64);
    case SType::FLOAT32: return _signbit<float>();
    case SType::FLOAT64: return _signbit<double>();
    default:
      throw TypeError() << "Function `signbit` cannot be applied to a "
                           "column of type `" << stype << "`";
  }
}




}}  // namespace dt::expr
