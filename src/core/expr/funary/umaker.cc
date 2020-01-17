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
#include <unordered_map>
#include "column/const.h"
#include "expr/funary/umaker.h"
#include "utils/assert.h"
#include "column.h"
namespace dt {
namespace expr {


umaker::~umaker() = default;


//------------------------------------------------------------------------------
// (Op, SType) -> umaker
//------------------------------------------------------------------------------

static std::unordered_map<size_t, umaker_ptr> umakers_library;

static const umaker_ptr& get_umaker(Op opcode, SType stype) {
  size_t id = ((static_cast<size_t>(opcode) - UNOP_FIRST) << 8) +
              static_cast<size_t>(stype);
  if (umakers_library.count(id) == 0) {
    // Note: these operations must be separate, otherwise if `resolve_op`
    //       throws an exception, there could be an empty entry in the
    //       umakers_library map, which later will lead to a seg.fault
    auto res = resolve_op(opcode, stype);
    umakers_library[id] = std::move(res);
  }
  return umakers_library[id];
}




//------------------------------------------------------------------------------
// Resolver main factory function
//------------------------------------------------------------------------------

umaker_ptr resolve_op(Op opcode, SType stype)
{
  switch (opcode) {
    // Basic
    case Op::UPLUS:     return resolve_op_uplus(stype);
    case Op::UMINUS:    return resolve_op_uminus(stype);
    case Op::UINVERT:   return resolve_op_uinvert(stype);
    case Op::LEN:       return resolve_op_len(stype);

    // Math: trigonometric
    case Op::SIN:       return resolve_op_sin(stype);
    case Op::COS:       return resolve_op_cos(stype);
    case Op::TAN:       return resolve_op_tan(stype);
    case Op::ARCSIN:    return resolve_op_arcsin(stype);
    case Op::ARCCOS:    return resolve_op_arccos(stype);
    case Op::ARCTAN:    return resolve_op_arctan(stype);
    case Op::DEG2RAD:   return resolve_op_deg2rad(stype);
    case Op::RAD2DEG:   return resolve_op_rad2deg(stype);

    // Math: hyperbolic
    case Op::SINH:      return resolve_op_sinh(stype);
    case Op::COSH:      return resolve_op_cosh(stype);
    case Op::TANH:      return resolve_op_tanh(stype);
    case Op::ARSINH:    return resolve_op_arsinh(stype);
    case Op::ARCOSH:    return resolve_op_arcosh(stype);
    case Op::ARTANH:    return resolve_op_artanh(stype);

    // Math: exponential/power
    case Op::CBRT:      return resolve_op_cbrt(stype);
    case Op::EXP:       return resolve_op_exp(stype);
    case Op::EXP2:      return resolve_op_exp2(stype);
    case Op::EXPM1:     return resolve_op_expm1(stype);
    case Op::LOG:       return resolve_op_log(stype);
    case Op::LOG10:     return resolve_op_log10(stype);
    case Op::LOG1P:     return resolve_op_log1p(stype);
    case Op::LOG2:      return resolve_op_log2(stype);
    case Op::SQRT:      return resolve_op_sqrt(stype);
    case Op::SQUARE:    return resolve_op_square(stype);

    // Math: special
    case Op::ERF:       return resolve_op_erf(stype);
    case Op::ERFC:      return resolve_op_erfc(stype);
    case Op::GAMMA:     return resolve_op_gamma(stype);
    case Op::LGAMMA:    return resolve_op_lgamma(stype);

    // Math: floating-point
    case Op::ISFINITE:  return resolve_op_isfinite(stype);
    case Op::ISINF:     return resolve_op_isinf(stype);
    case Op::ISNA:      return resolve_op_isna(stype);
    case Op::CEIL:      return resolve_op_ceil(stype);
    case Op::ABS:       return resolve_op_abs(stype);
    case Op::FABS:      return resolve_op_fabs(stype);
    case Op::FLOOR:     return resolve_op_floor(stype);
    case Op::RINT:      return resolve_op_rint(stype);
    case Op::SIGN:      return resolve_op_sign(stype);
    case Op::SIGNBIT:   return resolve_op_signbit(stype);
    case Op::TRUNC:     return resolve_op_trunc(stype);

    default: throw RuntimeError() << "Unknown unary op " << int(opcode);
  }
}




//------------------------------------------------------------------------------
// Main unaryop functions
//------------------------------------------------------------------------------

Column unaryop(Op opcode, Column&& col) {
  const auto& maker = get_umaker(opcode, col.stype());
  return maker->compute(std::move(col));
}


py::oobj unaryop(Op opcode, std::nullptr_t) {
  const auto& maker = get_umaker(opcode, SType::VOID);
  Column arg = Const_ColumnImpl::make_na_column(1);
  return maker->compute(std::move(arg)).get_element_as_pyobject(0);
}


py::oobj unaryop(Op opcode, bool value) {
  const auto& maker = get_umaker(opcode, SType::BOOL);
  Column arg = Const_ColumnImpl::make_bool_column(1, value);
  return maker->compute(std::move(arg)).get_element_as_pyobject(0);
}


py::oobj unaryop(Op opcode, int64_t value) {
  const auto& maker = get_umaker(opcode, SType::INT64);
  Column arg = Const_ColumnImpl::make_int_column(1, value, SType::INT64);
  return maker->compute(std::move(arg)).get_element_as_pyobject(0);
}


py::oobj unaryop(Op opcode, double value) {
  const auto& maker = get_umaker(opcode, SType::FLOAT64);
  Column arg = std::isnan(value)? Const_ColumnImpl::make_na_column(1)
                                : Const_ColumnImpl::make_float_column(1, value);
  return maker->compute(std::move(arg)).get_element_as_pyobject(0);
}


py::oobj unaryop(Op opcode, CString value) {
  const auto& maker = get_umaker(opcode, SType::STR32);
  Column arg = Const_ColumnImpl::make_string_column(1, value);
  return maker->compute(std::move(arg)).get_element_as_pyobject(0);
}




}}  // namespace dt::expr
