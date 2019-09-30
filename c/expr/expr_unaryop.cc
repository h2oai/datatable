//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018-2019
//------------------------------------------------------------------------------
#include <cmath>
#include "column/virtual.h"
#include "expr/expr_unaryop.h"
#include "frame/py_frame.h"
#include "parallel/api.h"
#include "column_impl.h"  // TODO: remove
#include "datatablemodule.h"
#include "types.h"
namespace dt {
namespace expr {

// Singleton instance
unary_infos unary_library;



//------------------------------------------------------------------------------
// Vector functions
//------------------------------------------------------------------------------

template<typename TI, typename TO, TO(*OP)(TI)>
void mapfw(size_t nrows, const TI* inp, TO* out) {
  dt::parallel_for_static(nrows,
    [=](size_t i) {
      out[i] = OP(inp[i]);
    });
}


template<typename TI, typename TO>
void map_str_len(size_t nrows, const TI* inp, TO* out) {
  inp++;
  dt::parallel_for_static(nrows,
    [=](size_t i) {
      out[i] = ISNA<TI>(inp[i])
                  ? GETNA<TO>()
                  : static_cast<TO>((inp[i] - inp[i-1]) & ~GETNA<TI>());
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




//------------------------------------------------------------------------------
// Virtual-column functions
//------------------------------------------------------------------------------

template <typename TI, typename TO>
class unary_vcol : public Virtual_ColumnImpl {
  using operator_t = TO(*)(TI);
  private:
    Column arg;
    operator_t func;

  public:
    unary_vcol(Column&& col, SType stype, operator_t f)
      : Virtual_ColumnImpl(col.nrows(), stype),
        arg(std::move(col)),
        func(f) {}

    ColumnImpl* shallowcopy() const override {
      return new unary_vcol<TI, TO>(Column(arg), _stype, func);
    }

    bool get_element(size_t i, TO* out) const override {
      TI x;
      bool isvalid = arg.get_element(i, &x);
      (void) isvalid;  // FIXME
      TO value = func(x);
      *out = value;
      return !ISNA<TO>(value);
    }
};

template <typename TI>
class unary_vcol<TI, int8_t> : public Virtual_ColumnImpl {
  using operator_t = int8_t(*)(TI);
  private:
    Column arg;
    operator_t func;

  public:
    unary_vcol(Column&& col, SType stype, operator_t f)
      : Virtual_ColumnImpl(col.nrows(), stype),
        arg(std::move(col)),
        func(f) {}

    ColumnImpl* shallowcopy() const override {
      return new unary_vcol<TI, int8_t>(Column(arg), _stype, func);
    }

    bool get_element(size_t i, int8_t* out) const override {
      TI x;
      bool isvalid = arg.get_element(i, &x);
      (void) isvalid;  // FIXME
      int8_t value = func(x);
      *out = value;
      return !ISNA<int8_t>(value);
    }

    bool get_element(size_t i, int32_t* out) const override {
      TI x;
      bool isvalid = arg.get_element(i, &x);
      (void) isvalid;  // FIXME
      int8_t value = func(x);
      *out = static_cast<int32_t>(value);
      return !ISNA<int8_t>(value);
    }
};



template <SType SI, SType SO, element_t<SO>(*FN)(element_t<SI>)>
Column vcol_factory(Column&& arg) {
  return Column(
      new unary_vcol<element_t<SI>, element_t<SO>>(std::move(arg), SO, FN)
  );
}


template <SType SI, bool(*FN)(element_t<SI>)>
Column vcol_factory_bool(Column&& arg) {
  using TI = element_t<SI>;
  return Column(
      new unary_vcol<TI, int8_t>(std::move(arg), SType::BOOL,
                                 reinterpret_cast<int8_t(*)(TI)>(FN))
  );
}


template <SType SO, element_t<SO>(*FN)(CString)>
Column vcol_factory_str(Column&& arg) {
  return Column(
      new unary_vcol<CString, element_t<SO>>(std::move(arg), SO, FN)
  );
}


static Column vcol_id(Column&& arg) {
  return std::move(arg);
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
inline static int8_t op_isna(T x) { return ISNA<T>(x); }

template <typename T>
inline static int8_t op_notna(T x) { return !ISNA<T>(x); }

template <typename T>
inline static int8_t op_false(T) { return false; }


template <typename T>
inline static T op_abs(T x) {
  // If T is floating point and x is NA, then (x < 0) will evaluate to false;
  // If T is integer and x is NA, then (x < 0) will be true, but -x will be
  // equal to x.
  // Thus, in all cases we'll have `abs(NA) == NA`.
  return (x < 0) ? -x : x;
}

// Redefine functions isinf / isfinite here, because apparently the standard
// library implementation is not that standard: in some implementation the
// return value is `bool`, in others it's `int`.
template <typename T>
inline static int8_t op_isinf(T x) { return std::isinf(x); }

template <typename T>
inline static int8_t op_isfinite(T x) { return std::isfinite(x); }

inline static int8_t op_invert_bool(int8_t x) {
  return ISNA<int8_t>(x)? x : !x;
}

template <typename T>
inline static T op_inverse(T x) {
  return ISNA<T>(x)? x : ~x;
}

inline static int8_t op_str_isna(CString str) {
  return (str.ch == nullptr);
}

template <typename T>
inline static T op_str_len(CString str) {
  return (str.ch == nullptr)? GETNA<T>() : static_cast<T>(str.size);
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


SType expr_unaryop::resolve(const EvalContext& ctx) {
  SType input_stype = arg->resolve(ctx);
  return unary_library.get_infox(opcode, input_stype).output_stype;
}


GroupbyMode expr_unaryop::get_groupby_mode(const EvalContext& ctx) const {
  return arg->get_groupby_mode(ctx);
}


Column expr_unaryop::evaluate(EvalContext& ctx) {
  Column input_column = arg->evaluate(ctx);

  auto input_stype = input_column.stype();
  const auto& ui = unary_library.get_infox(opcode, input_stype);
  if (ui.cast_stype != SType::VOID) {
    input_column.cast_inplace(ui.cast_stype);
  }
  if (ui.vectorfn == nullptr) {
    return input_column;
  }
  input_column.materialize();

  size_t nrows = input_column.nrows();
  const void* inp =  input_column.get_data_readonly();
  Column output_column = Column::new_data_column(nrows, ui.output_stype);
  void* out = output_column.get_data_editable();

  ui.vectorfn(nrows, inp, out);

  return output_column;
}


// Column expr_unaryop::evaluate(EvalContext& ctx) {
//   Column varg = arg->evaluate(ctx);
//   SType input_stype = varg.stype();
//   const auto& ui = unary_library.get_infox(opcode, input_stype);
//   if (ui.cast_stype != SType::VOID) {
//     varg = std::move(varg).cast(ui.cast_stype);
//     input_stype = ui.cast_stype;
//   }
//   if (!ui.vcolfn) {
//     throw NotImplError() << "Cannot create a virtual column for input_stype = "
//         << input_stype << " and op = " << static_cast<size_t>(opcode);
//   }
//   return ui.vcolfn(std::move(varg));
// }




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
    py::oobj col_selector = make_pyexpr(Op::COL,
                                        py::otuple{py::oint(i)},
                                        py::otuple{py::oint(0)});
    columns.set(i, make_pyexpr(opcode, col_selector));
  }

  py::oobj res = frame->m__getitem__(py::otuple{ py::None(), columns });
  DataTable* res_dt = res.to_datatable();
  res_dt->copy_names_from(dt);
  return res;
}


py::oobj unary_pyfn(const py::PKArgs& args) {
  using uinfo = unary_infos::uinfo;
  Op opcode = unary_library.get_opcode_from_args(args);

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
      auto res = reinterpret_cast<int64_t(*)(int64_t)>(info->scalarfn)(v);
      return py::oint(res);
    }
    if (info && info->output_stype == SType::BOOL) {
      auto res = reinterpret_cast<bool(*)(int64_t)>(info->scalarfn)(v);
      return py::obool(res);
    }
    goto process_as_float;
  }
  if (x.is_float() || x.is_none()) {
    process_as_float:
    double v = x.to_double();
    const uinfo* info = unary_library.get_infon(opcode, SType::FLOAT64);
    if (info && info->output_stype == SType::FLOAT64) {
      auto res = reinterpret_cast<double(*)(double)>(info->scalarfn)(v);
      return py::ofloat(res);
    }
    if (info && info->output_stype == SType::BOOL) {
      auto res = reinterpret_cast<bool(*)(double)>(info->scalarfn)(v);
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
using uinfo = unary_infos::uinfo;

const uinfo* unary_infos::get_infon(Op op, SType input_stype) const {
  size_t entry_id = id(op, input_stype);
  return info.count(entry_id)? &info.at(entry_id) : nullptr;
}

const uinfo& unary_infos::get_infox(Op op, SType input_stype) const {
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

Op unary_infos::get_opcode_from_args(const py::PKArgs& args) const {
  return opcodes.at(&args);
}



constexpr size_t unary_infos::id(Op op) noexcept {
  return static_cast<size_t>(op) - UNOP_FIRST;
}

constexpr size_t unary_infos::id(Op op, SType stype) noexcept {
  return static_cast<size_t>(op) * DT_STYPES_COUNT +
         static_cast<size_t>(stype);
}

void unary_infos::add_op(Op opcode, const char* name, const py::PKArgs* args) {
  names[id(opcode)] = name;
  opcodes[args] = opcode;
}


template <Op OP, SType SI, SType SO, element_t<SO>(*FN)(element_t<SI>)>
void unary_infos::add() {
  using TI = element_t<SI>;
  using TO = element_t<SO>;
  constexpr size_t entry_id = id(OP, SI);
  xassert(info.count(entry_id) == 0);
  info[entry_id] = {
    /* vectorfn = */ reinterpret_cast<unary_func_t>(mapfw<TI, TO, FN>),
    /* scalarfn = */ reinterpret_cast<erased_func_t>(FN),
    /* vcolfn =   */ vcol_factory<SI, SO, FN>,
    /* outstype = */ SO,
    /* caststype= */ SType::VOID
  };
}

template <Op OP, SType SI, SType SO, element_t<SO>(*FN)(CString)>
void unary_infos::add_str(unary_func_t mapfn)
{
  constexpr size_t entry_id = id(OP, SI);
  xassert(info.count(entry_id) == 0);
  info[entry_id] = {
    mapfn,
    reinterpret_cast<erased_func_t>(FN),
    vcol_factory_str<SO, FN>,
    SO,
    SType::VOID
  };
}

void unary_infos::add_copy(Op op, SType input_stype, SType output_stype) {
  size_t entry_id = id(op, input_stype);
  xassert(info.count(entry_id) == 0);
  SType cast_stype = (input_stype == output_stype)? SType::VOID : output_stype;
  info[entry_id] = {nullptr, nullptr, vcol_id, output_stype, cast_stype};
}


template <float(*F32)(float), double(*F64)(double)>
inline void unary_infos::add_math(
    Op opcode, const char* name, const py::PKArgs& args)
{
  static SType integer_stypes[] = {SType::BOOL, SType::INT8, SType::INT16,
                                   SType::INT32, SType::INT64};
  auto m32 = reinterpret_cast<unary_func_t>(mapfw<float,  float,  F32>);
  auto m64 = reinterpret_cast<unary_func_t>(mapfw<double, double, F64>);
  auto s32 = reinterpret_cast<erased_func_t>(F32);
  auto s64 = reinterpret_cast<erased_func_t>(F64);
  auto v32 = vcol_factory<SType::FLOAT32, SType::FLOAT32, F32>;
  auto v64 = vcol_factory<SType::FLOAT64, SType::FLOAT64, F64>;
  names[id(opcode)] = name;
  opcodes[&args] = opcode;
  for (size_t i = 0; i < 5; ++i) {
    size_t entry_id = id(opcode, integer_stypes[i]);
    xassert(info.count(entry_id) == 0);
    info[entry_id] = {m64, s64, v64, SType::FLOAT64, SType::FLOAT64};
  }
  size_t id_f32 = id(opcode, SType::FLOAT32);
  size_t id_f64 = id(opcode, SType::FLOAT64);
  xassert(info.count(id_f32) == 0 && info.count(id_f64) == 0);
  info[id_f32] = {m32, s32, v32, SType::FLOAT32, SType::VOID};
  info[id_f64] = {m64, s64, v64, SType::FLOAT64, SType::VOID};
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

  add_op(Op::UPLUS, "+", nullptr);
  add_copy(Op::UPLUS, bool8, int8);
  add_copy(Op::UPLUS, int8,  int8);
  add_copy(Op::UPLUS, int16, int16);
  add_copy(Op::UPLUS, int32, int32);
  add_copy(Op::UPLUS, int64, int64);
  add_copy(Op::UPLUS, flt32, flt32);
  add_copy(Op::UPLUS, flt64, flt64);

  add_op(Op::UMINUS, "-", nullptr);
  add<Op::UMINUS, bool8, int8,  op_minus<int8_t>>();
  add<Op::UMINUS, int8,  int8,  op_minus<int8_t>>();
  add<Op::UMINUS, int16, int16, op_minus<int16_t>>();
  add<Op::UMINUS, int32, int32, op_minus<int32_t>>();
  add<Op::UMINUS, int64, int64, op_minus<int64_t>>();
  add<Op::UMINUS, flt32, flt32, op_minus<float>>();
  add<Op::UMINUS, flt64, flt64, op_minus<double>>();

  add_op(Op::UINVERT, "~", nullptr);
  add<Op::UINVERT, bool8, bool8, op_invert_bool>();
  add<Op::UINVERT, int8,  int8,  op_inverse<int8_t>>();
  add<Op::UINVERT, int16, int16, op_inverse<int16_t>>();
  add<Op::UINVERT, int32, int32, op_inverse<int32_t>>();
  add<Op::UINVERT, int64, int64, op_inverse<int64_t>>();

  add_op(Op::ABS, "abs", &args_abs);
  add_copy(Op::ABS, bool8, int8);
  add<Op::ABS, int8,  int8,  op_abs<int8_t>>();
  add<Op::ABS, int16, int16, op_abs<int16_t>>();
  add<Op::ABS, int32, int32, op_abs<int32_t>>();
  add<Op::ABS, int64, int64, op_abs<int64_t>>();
  add<Op::ABS, flt32, flt32, std::abs>();
  add<Op::ABS, flt64, flt64, std::abs>();

  add_op(Op::ISNA, "isna", &args_isna);
  add<Op::ISNA, bool8, bool8, op_isna<int8_t>>();
  add<Op::ISNA, int8,  bool8, op_isna<int8_t>>();
  add<Op::ISNA, int16, bool8, op_isna<int16_t>>();
  add<Op::ISNA, int32, bool8, op_isna<int32_t>>();
  add<Op::ISNA, int64, bool8, op_isna<int64_t>>();
  add<Op::ISNA, flt32, bool8, op_isna<float>>();
  add<Op::ISNA, flt64, bool8, op_isna<double>>();
  add_str<Op::ISNA, str32, bool8, op_str_isna>(U(map_str_isna<uint32_t>));
  add_str<Op::ISNA, str64, bool8, op_str_isna>(U(map_str_isna<uint64_t>));

  add_op(Op::ISFINITE, "isfinite", &args_isfinite);
  add<Op::ISFINITE, bool8, bool8, op_notna<int8_t>>();
  add<Op::ISFINITE, int8,  bool8, op_notna<int8_t>>();
  add<Op::ISFINITE, int16, bool8, op_notna<int16_t>>();
  add<Op::ISFINITE, int32, bool8, op_notna<int32_t>>();
  add<Op::ISFINITE, int64, bool8, op_notna<int64_t>>();
  add<Op::ISFINITE, flt32, bool8, op_isfinite<float>>();
  add<Op::ISFINITE, flt64, bool8, op_isfinite<double>>();

  add_op(Op::ISINF, "isinf", &args_isinf);
  add<Op::ISINF, bool8, bool8, op_false<int8_t>>();
  add<Op::ISINF, int8,  bool8, op_false<int8_t>>();
  add<Op::ISINF, int16, bool8, op_false<int16_t>>();
  add<Op::ISINF, int32, bool8, op_false<int32_t>>();
  add<Op::ISINF, int64, bool8, op_false<int64_t>>();
  add<Op::ISINF, flt32, bool8, op_isinf<float>>();
  add<Op::ISINF, flt64, bool8, op_isinf<double>>();

  add_op(Op::CEIL, "ceil", &args_ceil);
  add_copy(Op::CEIL, bool8, flt64);
  add_copy(Op::CEIL, int8,  flt64);
  add_copy(Op::CEIL, int16, flt64);
  add_copy(Op::CEIL, int32, flt64);
  add_copy(Op::CEIL, int64, flt64);
  add<Op::CEIL, flt32, flt32, std::ceil>();
  add<Op::CEIL, flt64, flt64, std::ceil>();

  add_op(Op::FLOOR, "floor", &args_floor);
  add_copy(Op::FLOOR, bool8, flt64);
  add_copy(Op::FLOOR, int8,  flt64);
  add_copy(Op::FLOOR, int16, flt64);
  add_copy(Op::FLOOR, int32, flt64);
  add_copy(Op::FLOOR, int64, flt64);
  add<Op::FLOOR, flt32, flt32, std::floor>();
  add<Op::FLOOR, flt64, flt64, std::floor>();

  add_op(Op::TRUNC, "trunc", &args_trunc);
  add_copy(Op::TRUNC, bool8, flt64);
  add_copy(Op::TRUNC, int8,  flt64);
  add_copy(Op::TRUNC, int16, flt64);
  add_copy(Op::TRUNC, int32, flt64);
  add_copy(Op::TRUNC, int64, flt64);
  add<Op::TRUNC, flt32, flt32, std::trunc>();
  add<Op::TRUNC, flt64, flt64, std::trunc>();

  add_op(Op::LEN, "len", &args_len);
  add_str<Op::LEN, str32, int32, op_str_len<int32_t>>(U(map_str_len<uint32_t, int32_t>));
  add_str<Op::LEN, str64, int64, op_str_len<int64_t>>(U(map_str_len<uint64_t, int64_t>));

  add_math<&std::acos,  &std::acos> (Op::ARCCOS,  "arccos",  args_arccos);
  add_math<&std::acosh, &std::acosh>(Op::ARCOSH,  "arcosh",  args_arcosh);
  add_math<&std::asin,  &std::asin> (Op::ARCSIN,  "arcsin",  args_arcsin);
  add_math<&std::asinh, &std::asinh>(Op::ARSINH,  "arsinh",  args_arsinh);
  add_math<&std::atan,  &std::atan> (Op::ARCTAN,  "arctan",  args_arctan);
  add_math<&std::atanh, &std::atanh>(Op::ARTANH,  "artanh",  args_artanh);
  add_math<&std::cos,   &std::cos>  (Op::COS,     "cos",     args_cos);
  add_math<&std::cosh,  &std::cosh> (Op::COSH,    "cosh",    args_cosh);
  add_math<&fn_deg2rad, &fn_deg2rad>(Op::DEG2RAD, "deg2rad", args_deg2rad);
  add_math<&fn_rad2deg, &fn_rad2deg>(Op::RAD2DEG, "rad2deg", args_rad2deg);
  add_math<&std::sin,   &std::sin>  (Op::SIN,     "sin",     args_sin);
  add_math<&std::sinh,  &std::sinh> (Op::SINH,    "sinh",    args_sinh);
  add_math<&std::tan,   &std::tan>  (Op::TAN,     "tan",     args_tan);
  add_math<&std::tanh,  &std::tanh> (Op::TANH,    "tanh",    args_tanh);

  add_math<&std::cbrt,  &std::cbrt> (Op::CBRT,    "cbrt",    args_cbrt);
  add_math<&std::exp,   &std::exp>  (Op::EXP,     "exp",     args_exp);
  add_math<&std::exp2,  &std::exp2> (Op::EXP2,    "exp2",    args_exp2);
  add_math<&std::expm1, &std::expm1>(Op::EXPM1,   "expm1",   args_expm1);
  add_math<&std::log,   &std::log>  (Op::LOG,     "log",     args_log);
  add_math<&std::log10, &std::log10>(Op::LOG10,   "log10",   args_log10);
  add_math<&std::log1p, &std::log1p>(Op::LOG1P,   "log1p",   args_log1p);
  add_math<&std::log2,  &std::log2> (Op::LOG2,    "log2",    args_log2);
  add_math<&std::sqrt,  &std::sqrt> (Op::SQRT,    "sqrt",    args_sqrt);
  add_math<&fn_square,  &fn_square> (Op::SQUARE,  "square",  args_square);

  add_math<&std::erf,    &std::erf>   (Op::ERF,    "erf",    args_erf);
  add_math<&std::erfc,   &std::erfc>  (Op::ERFC,   "erfc",   args_erfc);
  add_math<&std::tgamma, &std::tgamma>(Op::GAMMA,  "gamma",  args_gamma);
  add_math<&std::lgamma, &std::lgamma>(Op::LGAMMA, "lgamma", args_lgamma);

  add_math<&std::fabs, &std::fabs>(Op::FABS, "fabs", args_fabs);
  add_math<&fn_sign,   &fn_sign>  (Op::SIGN, "sign", args_sign);

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
