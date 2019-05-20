//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018-2019
//------------------------------------------------------------------------------
#include <cmath>
#include "expr/expr_unaryop.h"
#include "frame/py_frame.h"
#include "parallel/api.h"
#include "datatablemodule.h"
#include "types.h"
namespace dt {
namespace expr {

// Singleton instance
unary_infos unary_library;



//------------------------------------------------------------------------------
// Mapper functions
//------------------------------------------------------------------------------

template<typename T, T(*OP)(T)>
void map1(size_t nrows, const T* inp, T* out) {
  dt::parallel_for_static(nrows,
    [=](size_t i) {
      out[i] = OP(inp[i]);
    });
}


template<typename IT, typename OT, OT(*OP)(IT)>
void map11(size_t nrows, const IT* inp, OT* out) {
  dt::parallel_for_static(nrows,
    [=](size_t i) {
      out[i] = OP(inp[i]);
    });
}


template<typename IT, typename OT>
void map_str_len(size_t nrows, const IT* inp, OT* out) {
  inp++;
  dt::parallel_for_static(nrows,
    [=](size_t i) {
      out[i] = ISNA<IT>(inp[i])
                  ? GETNA<OT>()
                  : static_cast<OT>((inp[i] - inp[i-1]) & ~GETNA<IT>());
    });
}


template<typename T>
void map_str_isna(size_t nrows, const T* inp, int8_t* out) {
  inp++;
  dt::parallel_for_static(nrows,
    [=](size_t i) {
      out[i] = ISNA<T>(inp[i]);
    });
}


template <int8_t VAL>
void set_const(size_t nrows, const void*, int8_t* out) {
  dt::parallel_for_static(nrows,
    [=](size_t i){
      out[i] = VAL;
    });
}




//------------------------------------------------------------------------------
// Operator implementations
//------------------------------------------------------------------------------

// If `x` is integer NA, then (-(INT_MIN)) == INT_MIN due to overflow.
// If `x` is floating-point NA, then (-NaN) == NaN by rules of IEEE754.
// Thus, in both cases (-NA)==NA, as desired.
template <typename T>
inline static T op_minus(T x) {
  return -x;
}

template <typename T>
inline static bool op_isna(T x) { return ISNA<T>(x); }

template <typename T>
inline static bool op_notna(T x) { return !ISNA<T>(x); }

template <typename T>
inline static bool op_false(T) { return false; }


template <typename T>
inline static T op_abs(T x) {
  // If T is floating point and x is NA, then (x < 0) will evaluate to false;
  // If T is integer and x is NA, then (x < 0) will be true, but -x will be
  // equal to x.
  // Thus, in all cases we'll have `abs(NA) == NA`.
  return (x < 0) ? -x : x;
}


inline static int8_t op_invert_bool(int8_t x) {
  return ISNA<int8_t>(x)? x : !x;
}

template <typename T>
inline static T op_inverse(T x) {
  return ISNA<T>(x)? x : ~x;
}


static double fn_rad2deg(double x) {
  static constexpr double DEGREES_IN_RADIAN = 57.295779513082323;
  return x * DEGREES_IN_RADIAN;
}
static float fn_rad2deg(float x) {
  static constexpr float DEGREES_IN_RADIAN = 57.2957802f;
  return x * DEGREES_IN_RADIAN;
}

static double fn_deg2rad(double x) {
  static constexpr double RADIANS_IN_DEGREE = 0.017453292519943295;
  return x * RADIANS_IN_DEGREE;
}
static float fn_deg2rad(float x) {
  static constexpr float RADIANS_IN_DEGREE = 0.0174532924f;
  return x * RADIANS_IN_DEGREE;
}

static float fn_square(float x) {
  return x * x;
}
static double fn_square(double x) {
  return x * x;
}

static double fn_sign(double x) {
  return (x > 0)? 1.0 : (x < 0)? -1.0 : (x == 0)? 0.0 : GETNA<double>();
}
static float fn_sign(float x) {
  return (x > 0)? 1.0f : (x < 0)? -1.0f : (x == 0)? 0.0f : GETNA<float>();
}




//------------------------------------------------------------------------------
// expr_unaryop
//------------------------------------------------------------------------------

expr_unaryop::expr_unaryop(pexpr&& a, Op op)
  : arg(std::move(a)), opcode(op) {}


bool expr_unaryop::is_negated_expr() const {
  return opcode == Op::UMINUS;
}

pexpr expr_unaryop::get_negated_expr() {
  pexpr res;
  if (opcode == Op::UMINUS) {
    res = std::move(arg);
  }
  return res;
}


SType expr_unaryop::resolve(const workframe& wf) {
  SType input_stype = arg->resolve(wf);
  return unary_library.get_infox(opcode, input_stype).output_stype;
}


GroupbyMode expr_unaryop::get_groupby_mode(const workframe& wf) const {
  return arg->get_groupby_mode(wf);
}


// In this method we use the following shortcut:
// if the `input_column` evaluated from `arg` is "writable" (meaning that
// its reference-counter is 1, it is allocated in RAM and not marker reaonly),
// then this column's memory buffer can be reused to create the output column.
// Indeed, if nobody else holds a reference to the same memory range, then it
// will be deallocated at the end of this function anyways, together with the
// `input_column`.
// This works only if each element in the output array corresponds to an
// element in the input at exactly the same memory location. Thus, this
// optimization is only applicable when the both columns are "fixed-width" and
// element sizes in the input and the output columns are the same. Notably,
// if the input is a string column, then it cannot be safely repurposed into
// an integer output column because each "element" of the input actually
// consists of 2 entries: the offsets of the start and the end of a string.
//
colptr expr_unaryop::evaluate_eager(workframe& wf) {
  auto input_column = arg->evaluate_eager(wf);

  auto input_stype = input_column->stype();
  const auto& ui = unary_library.get_infox(opcode, input_stype);
  if (ui.fn == nullptr) {
    if (input_stype == ui.output_stype) {
      return colptr(input_column->shallowcopy());
    } else {
      return colptr(input_column->cast(ui.output_stype));
    }
  }
  if (ui.cast_stype != SType::VOID && input_stype != ui.cast_stype) {
    input_column = colptr(input_column->cast(ui.cast_stype));
  }
  input_column->materialize();

  size_t nrows = input_column->nrows;
  const MemoryRange& input_mbuf = input_column->data_buf();
  const void* inp = input_mbuf.rptr();

  MemoryRange output_mbuf;
  void* out = nullptr;
  size_t out_elemsize = info(ui.output_stype).elemsize();
  if (input_mbuf.is_writable() &&
      input_column->is_fixedwidth() &&
      input_column->elemsize() == out_elemsize) {
    // The address `out` must be taken *before* `output_mbuf` is initialized,
    // since the `.xptr()` method checks that the reference counter is 1.
    out = input_mbuf.xptr();
    output_mbuf = MemoryRange(input_mbuf);
  }
  else {
    output_mbuf = MemoryRange::mem(out_elemsize * nrows);
    out = output_mbuf.xptr();
  }
  auto output_column = colptr(
      Column::new_mbuf_column(ui.output_stype, std::move(output_mbuf)));

  ui.fn(nrows, inp, out);

  return output_column;
}



//------------------------------------------------------------------------------
// unary_pyfn
//------------------------------------------------------------------------------

// This helper function will apply `opcode` to the entire frame, and
// return the resulting frame (same shape as the original).
static py::oobj process_frame(Op opcode, py::robj arg) {
  xassert(arg.is_frame());
  auto frame = static_cast<py::Frame*>(arg.to_borrowed_ref());
  DataTable* dt = frame->get_datatable();

  py::olist columns(dt->ncols);
  for (size_t i = 0; i < dt->ncols; ++i) {
    py::oobj col_selector = make_pyexpr(Op::COL, py::oint(0), py::oint(i));
    columns.set(i, make_pyexpr(opcode, col_selector));
  }

  py::oobj res = frame->m__getitem__(py::otuple{ py::None(), columns });
  DataTable* res_dt = res.to_datatable();
  res_dt->copy_names_from(dt);
  return res;
}


py::oobj unary_pyfn(const py::PKArgs& args) {
  using uinfo = unary_infos::uinfo;
  Op opcode = unary_library.resolve_opcode(args);

  py::robj x = args[0].to_robj();
  if (x.is_dtexpr()) {
    return make_pyexpr(opcode, x);
  }
  if (x.is_frame()) {
    return process_frame(opcode, x);
  }
  if (x.is_int()) {
    int64_t v = x.to_int64_strict();
    const uinfo* info = unary_library.get_infon(opcode, SType::INT64);
    if (info && info->output_stype == SType::INT64) {
      auto res = info->scalarfn.l_l(v);
      return py::oint(res);
    }
    if (info && info->output_stype == SType::BOOL) {
      auto res = info->scalarfn.l_b(v);
      return py::obool(res);
    }
    goto process_as_float;
  }
  if (x.is_float() || x.is_none()) {
    process_as_float:
    double v = x.to_double();
    const uinfo* info = unary_library.get_infon(opcode, SType::FLOAT64);
    if (info && info->output_stype == SType::FLOAT64) {
      auto res = info->scalarfn.d_d(v);
      return py::ofloat(res);
    }
    if (info && info->output_stype == SType::BOOL) {
      auto res = info->scalarfn.d_b(v);
      return py::obool(res);
    }
    if (!x.is_none()) {
      throw TypeError() << "Function `" << args.get_short_name()
        << "` cannot be applied to a numeric argument";
    }
    // Fall-through if x is None
  }
  if (x.is_string() || x.is_none()) {

  }
  if (!x) {
    throw TypeError() << "Function `" << args.get_short_name() << "()` takes "
        "exactly one argument, 0 given";
  }
  throw TypeError() << "Function `" << args.get_short_name() << "()` cannot "
      "be applied to an argument of type " << x.typeobj();
}



//------------------------------------------------------------------------------
// Trigonometric/hyperbolic functions
//------------------------------------------------------------------------------

static py::PKArgs args_arccos(
  1, 0, 0, false, false, {"x"}, "arccos",
R"(Inverse trigonometric cosine of x.

The returned value is in the interval [0, pi], or NA for those values of
x that lie outside the interval [-1, 1]. This function is the inverse of
cos() in the sense that `cos(arccos(x)) == x`.
)");

static py::PKArgs args_arcosh(
  1, 0, 0, false, false, {"x"}, "arcosh",
R"(Inverse hyperbolic cosine of x.)");

static py::PKArgs args_arcsin(
  1, 0, 0, false, false, {"x"}, "arcsin",
R"(Inverse trigonometric sine of x.

The returned value is in the interval [-pi/2, pi/2], or NA for those values of
x that lie outside the interval [-1, 1]. This function is the inverse of
sin() in the sense that `sin(arcsin(x)) == x`.
)");

static py::PKArgs args_arsinh(
  1, 0, 0, false, false, {"x"}, "arsinh",
R"(Inverse hyperbolic sine of x.)");

static py::PKArgs args_arctan(
  1, 0, 0, false, false, {"x"}, "arctan",
R"(Inverse trigonometric tangent of x.)");

static py::PKArgs args_arctan2(
  2, 0, 0, false, false, {"x", "y"}, "arctan2",
R"(Arc-tangent of y/x, taking into account the signs of x and y.

This function returns the measure of the angle between the ray O(x,y)
and the horizontal abscissae Ox. When both x and y are zero, this
function returns zero.)");

static py::PKArgs args_artanh(
  1, 0, 0, false, false, {"x"}, "artanh",
R"(Inverse hyperbolic tangent of x.)");

static py::PKArgs args_cos(
  1, 0, 0, false, false, {"x"}, "cos", "Trigonometric cosine of x.");

static py::PKArgs args_cosh(
  1, 0, 0, false, false, {"x"}, "cosh", "Hyperbolic cosine of x.");

static py::PKArgs args_sin(
  1, 0, 0, false, false, {"x"}, "sin", "Trigonometric sine of x.");

static py::PKArgs args_sinh(
  1, 0, 0, false, false, {"x"}, "sinh", "Hyperbolic sine of x.");

static py::PKArgs args_tan(
  1, 0, 0, false, false, {"x"}, "tan", "Trigonometric tangent of x.");

static py::PKArgs args_tanh(
  1, 0, 0, false, false, {"x"}, "tanh", "Hyperbolic tangent of x.");

static py::PKArgs args_rad2deg(
  1, 0, 0, false, false, {"x"}, "rad2deg",
  "Convert angle measured in radians into degrees.");

static py::PKArgs args_deg2rad(
  1, 0, 0, false, false, {"x"}, "deg2rad",
  "Convert angle measured in degrees into radians.");

static py::PKArgs args_hypot(
  2, 0, 0, false, false, {"x", "y"}, "hypot",
  "The length of the hypotenuse of a right triangle with sides x and y.");



//------------------------------------------------------------------------------
// Power/exponent functions
//------------------------------------------------------------------------------

static py::PKArgs args_cbrt(
  1, 0, 0, false, false, {"x"}, "cbrt", "Cubic root of x.");

static py::PKArgs args_exp(
  1, 0, 0, false, false, {"x"}, "exp",
"The Euler's constant (e = 2.71828...) raised to the power of x.");

static py::PKArgs args_exp2(
  1, 0, 0, false, false, {"x"}, "exp2", "Compute 2 raised to the power of x.");

static py::PKArgs args_expm1(
  1, 0, 0, false, false, {"x"}, "expm1",
R"(Compute e raised to the power of x, minus 1. This function is
equivalent to `exp(x) - 1`, but it is more accurate for arguments
`x` close to zero.)");

static py::PKArgs args_log(
  1, 0, 0, false, false, {"x"}, "log", "Natural logarithm of x.");

static py::PKArgs args_log10(
  1, 0, 0, false, false, {"x"}, "log10", "Base-10 logarithm of x.");

static py::PKArgs args_log1p(
  1, 0, 0, false, false, {"x"}, "log1p", "Natural logarithm of (1 + x).");

static py::PKArgs args_log2(
  1, 0, 0, false, false, {"x"}, "log2", "Base-2 logarithm of x.");

static py::PKArgs args_sqrt(
  1, 0, 0, false, false, {"x"}, "sqrt", "Square root of x.");

static py::PKArgs args_square(
  1, 0, 0, false, false, {"x"}, "square", "Square of x, i.e. same as x**2.");



//------------------------------------------------------------------------------
// Special mathematical functions
//------------------------------------------------------------------------------

static py::PKArgs args_erf(
  1, 0, 0, false, false, {"x"}, "erf",
R"(Error function erf(x).

The error function is defined as the integral
  erf(x) = 2/sqrt(pi) * Integrate[exp(-t**2), {t, 0, x}]
)");

static py::PKArgs args_erfc(
  1, 0, 0, false, false, {"x"}, "erfc",
R"(Complementary error function `erfc(x) = 1 - erf(x)`.

The complementary error function is defined as the integral
  erfc(x) = 2/sqrt(pi) * Integrate[exp(-t**2), {t, x, +inf}]

Although mathematically `erfc(x) = 1-erf(x)`, in practice the RHS
suffers catastrophic loss of precision at large values of `x`. This
function, however, does not have such drawback.
)");

static py::PKArgs args_gamma(
  1, 0, 0, false, false, {"x"}, "gamma",
R"(Euler Gamma function of x.

The gamma function is defined for all positive `x` as the integral
  gamma(x) = Integrate[t**(x-1) * exp(-t), {t, 0, +inf}]

In addition, for non-integer negative `x` the function is defined
via the relationship
  gamma(x) = gamma(x + k)/[x*(x+1)*...*(x+k-1)]
  where k = ceil(|x|)

If `x` is a positive integer, then `gamma(x) = (x - 1)!`.
)");

static py::PKArgs args_lgamma(
  1, 0, 0, false, false, {"x"}, "lgamma",
R"(Natural logarithm of absolute value of gamma function of x.)");




//------------------------------------------------------------------------------
// Floating-point functions
//------------------------------------------------------------------------------

static py::PKArgs args_isna(
  1, 0, 0, false, false, {"x"}, "isna",
  "Returns True if the argument is NA, and False otherwise.");

static py::PKArgs args_isfinite(
  1, 0, 0, false, false, {"x"}, "isfinite",
R"(Returns True if the argument has a finite value, and
False if the argument is +/- infinity or NaN.)");

static py::PKArgs args_isinf(
  1, 0, 0, false, false, {"x"}, "isinf",
  "Returns True if the argument is +/- infinity, and False otherwise.");




//------------------------------------------------------------------------------
// Miscellaneous functions
//------------------------------------------------------------------------------

static py::PKArgs args_fabs(
  1, 0, 0, false, false, {"x"}, "fabs",
  "Absolute value of x, returned as float64.");

static py::PKArgs args_sign(
  1, 0, 0, false, false, {"x"}, "sign",
R"(The sign of x, returned as float64.

This function returns 1 if x is positive (including positive
infinity), -1 if x is negative, 0 if x is zero, and NA if
x is NA.)");

static py::PKArgs args_abs(
  1, 0, 0, false, false, {"x"}, "abs",
  "Absolute value of a number.");

static py::PKArgs args_ceil(
  1, 0, 0, false, false, {"x"}, "ceil",
  "The smallest integer value not less than `x`.");

static py::PKArgs args_floor(
  1, 0, 0, false, false, {"x"}, "floor",
  "The largest integer value not greater than `x`.");

static py::PKArgs args_trunc(
  1, 0, 0, false, false, {"x"}, "trunc",
  "The nearest integer value not greater than `x` in absolute value.");

static py::PKArgs args_len(
  1, 0, 0, false, false, {"s"}, "len",
  "The length of the string `s`.");




//------------------------------------------------------------------------------
// unary_infos
//------------------------------------------------------------------------------

constexpr size_t unary_infos::id(Op op) noexcept {
  return static_cast<size_t>(op) - UNOP_FIRST;
}

constexpr size_t unary_infos::id(Op op, SType stype) noexcept {
  return static_cast<size_t>(op) * DT_STYPES_COUNT +
         static_cast<size_t>(stype);
}

void unary_infos::set_name(Op op, const std::string& name) {
  names[id(op)] = name;
}

void unary_infos::add(Op op, SType input_stype, SType output_stype,
                      unary_func_t fn, SType cast)
{
  size_t entry_id = id(op, input_stype);
  xassert(info.count(entry_id) == 0);
  info[entry_id] = {fn, {nullptr}, output_stype, cast};
}


const unary_infos::uinfo* unary_infos::get_infon(Op op, SType input_stype) const
{
  size_t entry_id = id(op, input_stype);
  return info.count(entry_id)? &info.at(entry_id) : nullptr;
}


const unary_infos::uinfo& unary_infos::get_infox(Op op, SType input_stype) const
{
  size_t entry_id = id(op, input_stype);
  if (info.count(entry_id)) {
    return info.at(entry_id);
  }
  size_t name_id = id(op);
  const std::string& opname = names.count(name_id)? names.at(name_id) : "";
  auto err = TypeError();
  err << "Cannot apply ";
  if (op == Op::UPLUS || op == Op::UMINUS || op == Op::UINVERT) {
    err << "unary `operator " << opname << "`";
  } else {
    err << "function `" << opname << "()`";
  }
  err << " to a column with stype `" << input_stype << "`";
  throw err;
}


Op unary_infos::resolve_opcode(const py::PKArgs& args) const {
  return opcodes.at(&args);
}


void unary_infos::register_op(
  Op opcode, const std::string& name, const py::PKArgs& args,
  dt::function<void()> more)
{
  names[id(opcode)] = name;
  opcodes[&args] = opcode;
  current_opcode = opcode;
  more();
}

void unary_infos::add_converter(SType in, SType out, unary_func_t fn) {
  add(current_opcode, in, out, fn);
}

void unary_infos::add_copy_converter(SType in, SType out) {
  if (out == SType::VOID) out = in;
  add(current_opcode, in, out, nullptr);
}

void unary_infos::add_converter(void(*f)(size_t, const int8_t*, int8_t*)) {
  add(current_opcode, SType::INT8, SType::INT8, reinterpret_cast<unary_func_t>(f));
}

void unary_infos::add_converter(void(*f)(size_t, const int16_t*, int16_t*)) {
  add(current_opcode, SType::INT16, SType::INT16, reinterpret_cast<unary_func_t>(f));
}

void unary_infos::add_converter(void(*f)(size_t, const int32_t*, int32_t*)) {
  add(current_opcode, SType::INT32, SType::INT32, reinterpret_cast<unary_func_t>(f));
}

void unary_infos::add_converter(void(*f)(size_t, const int64_t*, int64_t*)) {
  add(current_opcode, SType::INT64, SType::INT64, reinterpret_cast<unary_func_t>(f));
}

void unary_infos::add_converter(void(*f)(size_t, const float*, float*)) {
  add(current_opcode, SType::FLOAT32, SType::FLOAT32, reinterpret_cast<unary_func_t>(f));
}

void unary_infos::add_converter(void(*f)(size_t, const double*, double*)) {
  add(current_opcode, SType::FLOAT64, SType::FLOAT64, reinterpret_cast<unary_func_t>(f));
}


void unary_infos::add_scalarfn_l(int64_t(*f)(int64_t)) {
  size_t entry_id = id(current_opcode, SType::INT64);
  xassert(info.count(entry_id) && !info[entry_id].scalarfn.l_l);
  info[entry_id].scalarfn.l_l = f;
}

void unary_infos::add_scalarfn_l(bool(*f)(int64_t)) {
  size_t entry_id = id(current_opcode, SType::INT64);
  xassert(info.count(entry_id) && !info[entry_id].scalarfn.l_b);
  info[entry_id].scalarfn.l_b = f;
}

void unary_infos::add_scalarfn_d(double(*f)(double)) {
  size_t entry_id = id(current_opcode, SType::FLOAT64);
  xassert(info.count(entry_id) && !info[entry_id].scalarfn.d_d);
  info[entry_id].scalarfn.d_d = f;
}

void unary_infos::add_scalarfn_d(bool(*f)(double)) {
  size_t entry_id = id(current_opcode, SType::FLOAT64);
  xassert(info.count(entry_id) && !info[entry_id].scalarfn.d_b);
  info[entry_id].scalarfn.d_b = f;
}

template <float(*F32)(float), double(*F64)(double)>
inline void unary_infos::register_math_op(
    Op opcode, const std::string& name, const py::PKArgs& args)
{
  auto f32 = reinterpret_cast<unary_func_t>(map1<float, F32>);
  auto f64 = reinterpret_cast<unary_func_t>(map1<double, F64>);
  names[id(opcode)] = name;
  opcodes[&args] = opcode;
  current_opcode = opcode;
  constexpr SType float64 = SType::FLOAT64;
  constexpr SType float32 = SType::FLOAT32;
  add(opcode, SType::BOOL,  float64, f64, float64);
  add(opcode, SType::INT8,  float64, f64, float64);
  add(opcode, SType::INT16, float64, f64, float64);
  add(opcode, SType::INT32, float64, f64, float64);
  add(opcode, SType::INT64, float64, f64, float64);
  add_converter(float32, float32, f32);
  add_converter(float64, float64, f64);
  add_scalarfn_d(F64);
}


unary_infos::unary_infos() {
  #define U reinterpret_cast<unary_func_t>
  constexpr SType bool8 = SType::BOOL;
  constexpr SType int8  = SType::INT8;
  constexpr SType int16 = SType::INT16;
  constexpr SType int32 = SType::INT32;
  constexpr SType int64 = SType::INT64;
  constexpr SType flt32 = SType::FLOAT32;
  constexpr SType flt64 = SType::FLOAT64;
  constexpr SType str32 = SType::STR32;
  constexpr SType str64 = SType::STR64;

  set_name(Op::UPLUS, "+");
  add(Op::UPLUS, bool8, int8,  nullptr);
  add(Op::UPLUS, int8,  int8,  nullptr);
  add(Op::UPLUS, int16, int16, nullptr);
  add(Op::UPLUS, int32, int32, nullptr);
  add(Op::UPLUS, int64, int64, nullptr);
  add(Op::UPLUS, flt32, flt32, nullptr);
  add(Op::UPLUS, flt64, flt64, nullptr);

  set_name(Op::UMINUS, "-");
  add(Op::UMINUS, bool8, int8,  U(map11<int8_t,  int8_t,  op_minus<int8_t>>));
  add(Op::UMINUS, int8,  int8,  U(map11<int8_t,  int8_t,  op_minus<int8_t>>));
  add(Op::UMINUS, int16, int16, U(map11<int16_t, int16_t, op_minus<int16_t>>));
  add(Op::UMINUS, int32, int32, U(map11<int32_t, int32_t, op_minus<int32_t>>));
  add(Op::UMINUS, int64, int64, U(map11<int64_t, int64_t, op_minus<int64_t>>));
  add(Op::UMINUS, flt32, flt32, U(map11<float,   float,   op_minus<float>>));
  add(Op::UMINUS, flt64, flt64, U(map11<double,  double,  op_minus<double>>));

  set_name(Op::UINVERT, "~");
  add(Op::UINVERT, bool8, bool8, U(map11<int8_t,  int8_t,  op_invert_bool>));
  add(Op::UINVERT, int8,  int8,  U(map11<int8_t,  int8_t,  op_inverse<int8_t>>));
  add(Op::UINVERT, int16, int16, U(map11<int16_t, int16_t, op_inverse<int16_t>>));
  add(Op::UINVERT, int32, int32, U(map11<int32_t, int32_t, op_inverse<int32_t>>));
  add(Op::UINVERT, int64, int64, U(map11<int64_t, int64_t, op_inverse<int64_t>>));

  register_op(Op::ABS, "abs", args_abs,
    [=]{
      add_copy_converter(bool8, int8);
      add_converter(map1<int8_t,  op_abs<int8_t>>);
      add_converter(map1<int16_t, op_abs<int16_t>>);
      add_converter(map1<int32_t, op_abs<int32_t>>);
      add_converter(map1<int64_t, op_abs<int64_t>>);
      add_converter(map1<float,   std::abs>);
      add_converter(map1<double,  std::abs>);
      add_scalarfn_l(op_abs<int64_t>);
      add_scalarfn_d(std::abs);
    });

  register_op(Op::ISNA, "isna", args_isna,
    [=]{
      add_converter(bool8, bool8, U(map11<int8_t,  bool, op_isna<int8_t>>));
      add_converter(int8,  bool8, U(map11<int8_t,  bool, op_isna<int8_t>>));
      add_converter(int16, bool8, U(map11<int16_t, bool, op_isna<int16_t>>));
      add_converter(int32, bool8, U(map11<int32_t, bool, op_isna<int32_t>>));
      add_converter(int64, bool8, U(map11<int64_t, bool, op_isna<int64_t>>));
      add_converter(flt32, bool8, U(map11<float,   bool, std::isnan>));
      add_converter(flt64, bool8, U(map11<double,  bool, std::isnan>));
      add_converter(str32, bool8, U(map_str_isna<uint32_t>));
      add_converter(str64, bool8, U(map_str_isna<uint64_t>));
      add_scalarfn_l(op_isna<int64_t>);
      add_scalarfn_d(std::isnan);
    });

  register_op(Op::ISFINITE, "isfinite", args_isfinite,
    [&]{
      add_converter(bool8, bool8, U(map11<int8_t,  bool, op_notna<int8_t>>));
      add_converter(int8,  bool8, U(map11<int8_t,  bool, op_notna<int8_t>>));
      add_converter(int16, bool8, U(map11<int16_t, bool, op_notna<int16_t>>));
      add_converter(int32, bool8, U(map11<int32_t, bool, op_notna<int32_t>>));
      add_converter(int64, bool8, U(map11<int64_t, bool, op_notna<int64_t>>));
      add_converter(flt32, bool8, U(map11<float,   bool, std::isfinite>));
      add_converter(flt64, bool8, U(map11<double,  bool, std::isfinite>));
      add_scalarfn_l(op_notna<int64_t>);
      add_scalarfn_d(std::isfinite);
    });

  register_op(Op::ISINF, "isinf", args_isinf,
    [&]{
      add_converter(bool8, bool8, U(map11<int8_t,  bool, op_false<int8_t>>));
      add_converter(int8,  bool8, U(map11<int8_t,  bool, op_false<int8_t>>));
      add_converter(int16, bool8, U(map11<int16_t, bool, op_false<int16_t>>));
      add_converter(int32, bool8, U(map11<int32_t, bool, op_false<int32_t>>));
      add_converter(int64, bool8, U(map11<int64_t, bool, op_false<int64_t>>));
      add_converter(flt32, bool8, U(map11<float,   bool, std::isinf>));
      add_converter(flt64, bool8, U(map11<double,  bool, std::isinf>));
      add_scalarfn_l(op_false<int64_t>);
      add_scalarfn_d(std::isinf);
    });

  register_op(Op::CEIL, "ceil", args_ceil,
    [&]{
      add_copy_converter(bool8, flt64);
      add_copy_converter(int8,  flt64);
      add_copy_converter(int16, flt64);
      add_copy_converter(int32, flt64);
      add_copy_converter(int64, flt64);
      add_converter(flt32, flt32, U(map11<float,  float,  std::ceil>));
      add_converter(flt64, flt64, U(map11<double, double, std::ceil>));
      add_scalarfn_d(std::ceil);
    });

  register_op(Op::FLOOR, "floor", args_floor,
    [&]{
      add_copy_converter(bool8, flt64);
      add_copy_converter(int8,  flt64);
      add_copy_converter(int16, flt64);
      add_copy_converter(int32, flt64);
      add_copy_converter(int64, flt64);
      add_converter(flt32, flt32, U(map11<float,  float,  std::floor>));
      add_converter(flt64, flt64, U(map11<double, double, std::floor>));
      add_scalarfn_d(std::floor);
    });

  register_op(Op::TRUNC, "trunc", args_trunc,
    [&]{
      add_copy_converter(bool8, flt64);
      add_copy_converter(int8,  flt64);
      add_copy_converter(int16, flt64);
      add_copy_converter(int32, flt64);
      add_copy_converter(int64, flt64);
      add_converter(flt32, flt32, U(map11<float,  float,  std::trunc>));
      add_converter(flt64, flt64, U(map11<double, double, std::trunc>));
      add_scalarfn_d(std::trunc);
    });

  register_op(Op::LEN, "len", args_len,
    [&]{
      add_converter(str32, int32, U(map_str_len<uint32_t, int32_t>));
      add_converter(str64, int64, U(map_str_len<uint64_t, int64_t>));
    });

  set_name(Op::FLOOR, "floor");
  set_name(Op::TRUNC, "trunc");

  register_math_op<&std::acos,  &std::acos> (Op::ARCCOS,  "arccos",  args_arccos);
  register_math_op<&std::acosh, &std::acosh>(Op::ARCOSH,  "arcosh",  args_arcosh);
  register_math_op<&std::asin,  &std::asin> (Op::ARCSIN,  "arcsin",  args_arcsin);
  register_math_op<&std::asinh, &std::asinh>(Op::ARSINH,  "arsinh",  args_arsinh);
  register_math_op<&std::atan,  &std::atan> (Op::ARCTAN,  "arctan",  args_arctan);
  register_math_op<&std::atanh, &std::atanh>(Op::ARTANH,  "artanh",  args_artanh);
  register_math_op<&std::cos,   &std::cos>  (Op::COS,     "cos",     args_cos);
  register_math_op<&std::cosh,  &std::cosh> (Op::COSH,    "cosh",    args_cosh);
  register_math_op<&fn_deg2rad, &fn_deg2rad>(Op::DEG2RAD, "deg2rad", args_deg2rad);
  register_math_op<&fn_rad2deg, &fn_rad2deg>(Op::RAD2DEG, "rad2deg", args_rad2deg);
  register_math_op<&std::sin,   &std::sin>  (Op::SIN,     "sin",     args_sin);
  register_math_op<&std::sinh,  &std::sinh> (Op::SINH,    "sinh",    args_sinh);
  register_math_op<&std::tan,   &std::tan>  (Op::TAN,     "tan",     args_tan);
  register_math_op<&std::tanh,  &std::tanh> (Op::TANH,    "tanh",    args_tanh);

  register_math_op<&std::cbrt,  &std::cbrt> (Op::CBRT,    "cbrt",    args_cbrt);
  register_math_op<&std::exp,   &std::exp>  (Op::EXP,     "exp",     args_exp);
  register_math_op<&std::exp2,  &std::exp2> (Op::EXP2,    "exp2",    args_exp2);
  register_math_op<&std::expm1, &std::expm1>(Op::EXPM1,   "expm1",   args_expm1);
  register_math_op<&std::log,   &std::log>  (Op::LOG,     "log",     args_log);
  register_math_op<&std::log10, &std::log10>(Op::LOG10,   "log10",   args_log10);
  register_math_op<&std::log1p, &std::log1p>(Op::LOG1P,   "log1p",   args_log1p);
  register_math_op<&std::log2,  &std::log2> (Op::LOG2,    "log2",    args_log2);
  register_math_op<&std::sqrt,  &std::sqrt> (Op::SQRT,    "sqrt",    args_sqrt);
  register_math_op<&fn_square,  &fn_square> (Op::SQUARE,  "square",  args_square);

  register_math_op<&std::erf,    &std::erf>   (Op::ERF,    "erf",    args_erf);
  register_math_op<&std::erfc,   &std::erfc>  (Op::ERFC,   "erfc",   args_erfc);
  register_math_op<&std::tgamma, &std::tgamma>(Op::GAMMA,  "gamma",  args_gamma);
  register_math_op<&std::lgamma, &std::lgamma>(Op::LGAMMA, "lgamma", args_lgamma);

  register_math_op<&std::fabs, &std::fabs>(Op::FABS, "fabs", args_fabs);
  register_math_op<&fn_sign,   &fn_sign>  (Op::SIGN, "sign", args_sign);

  #undef U
}

}}  // namespace dt::expr


void py::DatatableModule::init_unops() {
  using namespace dt::expr;
  ADD_FN(&unary_pyfn, args_abs);
  ADD_FN(&unary_pyfn, args_arccos);
  ADD_FN(&unary_pyfn, args_arcosh);
  ADD_FN(&unary_pyfn, args_arcsin);
  ADD_FN(&unary_pyfn, args_arctan);
  ADD_FN(&unary_pyfn, args_arsinh);
  ADD_FN(&unary_pyfn, args_artanh);
  ADD_FN(&unary_pyfn, args_cbrt);
  ADD_FN(&unary_pyfn, args_ceil);
  ADD_FN(&unary_pyfn, args_cos);
  ADD_FN(&unary_pyfn, args_cosh);
  ADD_FN(&unary_pyfn, args_deg2rad);
  ADD_FN(&unary_pyfn, args_erf);
  ADD_FN(&unary_pyfn, args_erfc);
  ADD_FN(&unary_pyfn, args_exp);
  ADD_FN(&unary_pyfn, args_exp2);
  ADD_FN(&unary_pyfn, args_expm1);
  ADD_FN(&unary_pyfn, args_fabs);
  ADD_FN(&unary_pyfn, args_floor);
  ADD_FN(&unary_pyfn, args_gamma);
  ADD_FN(&unary_pyfn, args_isfinite);
  ADD_FN(&unary_pyfn, args_isinf);
  ADD_FN(&unary_pyfn, args_isna);
  ADD_FN(&unary_pyfn, args_lgamma);
  ADD_FN(&unary_pyfn, args_log);
  ADD_FN(&unary_pyfn, args_log10);
  ADD_FN(&unary_pyfn, args_log1p);
  ADD_FN(&unary_pyfn, args_log2);
  ADD_FN(&unary_pyfn, args_rad2deg);
  ADD_FN(&unary_pyfn, args_sign);
  ADD_FN(&unary_pyfn, args_sin);
  ADD_FN(&unary_pyfn, args_sinh);
  ADD_FN(&unary_pyfn, args_sqrt);
  ADD_FN(&unary_pyfn, args_square);
  ADD_FN(&unary_pyfn, args_tan);
  ADD_FN(&unary_pyfn, args_tanh);
  ADD_FN(&unary_pyfn, args_trunc);
}
