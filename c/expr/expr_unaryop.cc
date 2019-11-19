//------------------------------------------------------------------------------
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
//
// © H2O.ai 2018-2019
//------------------------------------------------------------------------------
#include <cmath>
#include "column/func_unary.h"
#include "column/virtual.h"
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
// Virtual-column functions
//------------------------------------------------------------------------------


template <SType SI, SType SO, element_t<SO>(*FN)(element_t<SI>)>
Column vcol_factory(Column&& arg) {
  size_t nrows = arg.nrows();
  return Column(
      new FuncUnary1_ColumnImpl<element_t<SI>, element_t<SO>>(
              std::move(arg), FN, nrows, SO)
  );
}


template <SType SI, SType SO, bool(*FN)(read_t<SI>, bool, read_t<SO>*)>
Column vcol_factory2(Column&& arg) {
  size_t nrows = arg.nrows();
  return Column(
      new FuncUnary2_ColumnImpl<read_t<SI>, read_t<SO>>(
              std::move(arg), FN, nrows, SO)
  );
}


template <SType SI, bool(*FN)(element_t<SI>)>
Column vcol_factory_bool(Column&& arg) {
  using TI = element_t<SI>;
  size_t nrows = arg.nrows();
  return Column(
      new FuncUnary1_ColumnImpl<TI, int8_t>(
              std::move(arg), reinterpret_cast<int8_t(*)(TI)>(FN),
              nrows, SType::BOOL)
  );
}


static Column vcol_id(Column&& arg) {
  return std::move(arg);
}




//------------------------------------------------------------------------------
// Operator implementations
//------------------------------------------------------------------------------


[[ noreturn ]] inline static void op_notimpl() {
  throw NotImplError() << "This operation is not implemented yet";
}



template <typename T>
inline static bool op_isna(T, bool isvalid, int8_t* out) {
  *out = !isvalid;
  return true;
}


template <typename T>
inline static bool op_notna(T, bool isvalid, int8_t* out) {
  *out = isvalid;
  return true;
}


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
inline static bool op_isfinite(T x, bool isvalid, int8_t* out) {
  *out = isvalid && std::isfinite(x);
  return true;
}



/*
inline static bool op_str_len_ascii(CString str, bool isvalid, int64_t* out) {
  *out = str.size;
  return isvalid;
}
*/

static bool op_str_len_unicode(CString str, bool isvalid, int64_t* out) {
  if (isvalid) {
    int64_t len = 0;
    const uint8_t* ch = reinterpret_cast<const uint8_t*>(str.ch);
    const uint8_t* end = ch + str.size;
    while (ch < end) {
      uint8_t c = *ch;
      ch += (c < 0x80)? 1 :
            ((c & 0xE0) == 0xC0)? 2 :
            ((c & 0xF0) == 0xE0)? 3 :  4;
      len++;
    }
    *out = len;
  }
  return isvalid;
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
// unary_pyfn
//------------------------------------------------------------------------------

static py::oobj make_pyexpr(Op opcode, py::oobj arg) {
  size_t op = static_cast<size_t>(opcode);
  return py::robj(py::Expr_Type).call({ py::oint(op),
                                        py::otuple(arg) });
}

static py::oobj make_pyexpr(Op opcode, py::otuple args, py::otuple params) {
  size_t op = static_cast<size_t>(opcode);
  return py::robj(py::Expr_Type).call({ py::oint(op), args, params });
}


// This helper function will apply `opcode` to the entire frame, and
// return the resulting frame (same shape as the original).
static py::oobj process_frame(Op opcode, py::robj arg) {
  xassert(arg.is_frame());
  auto frame = static_cast<py::Frame*>(arg.to_borrowed_ref());
  DataTable* dt = frame->get_datatable();

  py::olist columns(dt->ncols());
  for (size_t i = 0; i < dt->ncols(); ++i) {
    py::oobj col_selector = make_pyexpr(Op::COL,
                                        py::otuple{py::oint(i)},
                                        py::otuple{py::oint(0)});
    columns.set(i, make_pyexpr(opcode, col_selector));
  }

  py::oobj res = frame->m__getitem__(py::otuple{ py::None(), columns });
  DataTable* res_dt = res.to_datatable();
  res_dt->copy_names_from(*dt);
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

static py::PKArgs args_arctan2(
  2, 0, 0, false, false, {"x", "y"}, "arctan2",
R"(Arc-tangent of y/x, taking into account the signs of x and y.

This function returns the measure of the angle between the ray O(x,y)
and the horizontal abscissae Ox. When both x and y are zero, this
function returns zero.)");

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


template <Op OP, SType SI, SType SO, read_t<SO>(*FN)(read_t<SI>)>
void unary_infos::add() {
  constexpr size_t entry_id = id(OP, SI);
  xassert(info.count(entry_id) == 0);
  info[entry_id] = {
    /* scalarfn = */ reinterpret_cast<erased_func_t>(FN),
    /* vcolfn =   */ vcol_factory<SI, SO, FN>,
    /* outstype = */ SO,
    /* caststype= */ SType::VOID
  };
}


template <Op OP, SType SI, SType SO, bool(*FN)(read_t<SI>, bool, read_t<SO>*)>
void unary_infos::add() {
  constexpr size_t entry_id = id(OP, SI);
  xassert(info.count(entry_id) == 0);
  info[entry_id] = {
    /* scalarfn = */ op_notimpl,
    /* vcolfn =   */ vcol_factory2<SI, SO, FN>,
    /* outstype = */ SO,
    /* caststype= */ SType::VOID
  };
}


void unary_infos::add_copy(Op op, SType input_stype, SType output_stype) {
  size_t entry_id = id(op, input_stype);
  xassert(info.count(entry_id) == 0);
  SType cast_stype = (input_stype == output_stype)? SType::VOID : output_stype;
  info[entry_id] = {nullptr, vcol_id, output_stype, cast_stype};
}


template <float(*F32)(float), double(*F64)(double)>
inline void unary_infos::add_math(
    Op opcode, const char* name, const py::PKArgs& args)
{
  static SType integer_stypes[] = {SType::BOOL, SType::INT8, SType::INT16,
                                   SType::INT32, SType::INT64};
  auto s32 = reinterpret_cast<erased_func_t>(F32);
  auto s64 = reinterpret_cast<erased_func_t>(F64);
  auto v32 = vcol_factory<SType::FLOAT32, SType::FLOAT32, F32>;
  auto v64 = vcol_factory<SType::FLOAT64, SType::FLOAT64, F64>;
  names[id(opcode)] = name;
  opcodes[&args] = opcode;
  for (size_t i = 0; i < 5; ++i) {
    size_t entry_id = id(opcode, integer_stypes[i]);
    xassert(info.count(entry_id) == 0);
    info[entry_id] = {s64, v64, SType::FLOAT64, SType::FLOAT64};
  }
  size_t id_f32 = id(opcode, SType::FLOAT32);
  size_t id_f64 = id(opcode, SType::FLOAT64);
  xassert(info.count(id_f32) == 0 && info.count(id_f64) == 0);
  info[id_f32] = {s32, v32, SType::FLOAT32, SType::VOID};
  info[id_f64] = {s64, v64, SType::FLOAT64, SType::VOID};
}


unary_infos::unary_infos() {
  constexpr SType bool8 = SType::BOOL;
  constexpr SType int8  = SType::INT8;
  constexpr SType int16 = SType::INT16;
  constexpr SType int32 = SType::INT32;
  constexpr SType int64 = SType::INT64;
  constexpr SType flt32 = SType::FLOAT32;
  constexpr SType flt64 = SType::FLOAT64;
  constexpr SType str32 = SType::STR32;
  constexpr SType str64 = SType::STR64;

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
  add<Op::ISNA, str32, bool8, op_isna<CString>>();
  add<Op::ISNA, str64, bool8, op_isna<CString>>();

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
  add<Op::LEN, str32, int64, op_str_len_unicode>();
  add<Op::LEN, str64, int64, op_str_len_unicode>();

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
}

}}  // namespace dt::expr


void py::DatatableModule::init_unops() {
  using namespace dt::expr;
  ADD_FN(&unary_pyfn, args_abs);
  ADD_FN(&unary_pyfn, args_cbrt);
  ADD_FN(&unary_pyfn, args_ceil);
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
  ADD_FN(&unary_pyfn, args_sign);
  ADD_FN(&unary_pyfn, args_sqrt);
  ADD_FN(&unary_pyfn, args_square);
  ADD_FN(&unary_pyfn, args_trunc);
}
