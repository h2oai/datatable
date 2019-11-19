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
#include "expr/funary/umaker.h"
#include "expr/expr_unaryop.h"   // TODO: remove
#include "utils/assert.h"
#include "column.h"
namespace dt {
namespace expr {


//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

umaker::~umaker() = default;


static constexpr size_t make_id(Op opcode, SType stype) noexcept {
  return ((static_cast<size_t>(opcode) - UNOP_FIRST) << 8) +
         static_cast<size_t>(stype);
}

static std::unordered_map<size_t, umaker_ptr> umakers_library;


umaker_ptr resolve_op(Op opcode, SType stype)
{
  switch (opcode) {
    // Basic
    case Op::UPLUS:      return resolve_op_uplus(stype);
    case Op::UMINUS:     return resolve_op_uminus(stype);
    case Op::UINVERT:    return resolve_op_uinvert(stype);

    // Math: trigonometric
    case Op::SIN:        return resolve_op_sin(stype);
    case Op::COS:        return resolve_op_cos(stype);
    case Op::TAN:        return resolve_op_tan(stype);
    case Op::ARCSIN:     return resolve_op_arcsin(stype);
    case Op::ARCCOS:     return resolve_op_arccos(stype);
    case Op::ARCTAN:     return resolve_op_arctan(stype);
    case Op::DEG2RAD:    return resolve_op_deg2rad(stype);
    case Op::RAD2DEG:    return resolve_op_rad2deg(stype);

    // Math: hyperbolic
    case Op::SINH:       return resolve_op_sinh(stype);
    case Op::COSH:       return resolve_op_cosh(stype);
    case Op::TANH:       return resolve_op_tanh(stype);
    case Op::ARSINH:     return resolve_op_arsinh(stype);
    case Op::ARCOSH:     return resolve_op_arcosh(stype);
    case Op::ARTANH:     return resolve_op_artanh(stype);

    // Math: exponential/power
    case Op::CBRT:       return umaker_ptr();
    case Op::EXP:        return umaker_ptr();
    case Op::EXP2:       return umaker_ptr();
    case Op::EXPM1:      return umaker_ptr();
    case Op::LOG:        return umaker_ptr();
    case Op::LOG10:      return umaker_ptr();
    case Op::LOG1P:      return umaker_ptr();
    case Op::LOG2:       return umaker_ptr();
    case Op::LOGADDEXP:  return umaker_ptr();
    case Op::LOGADDEXP2: return umaker_ptr();
    case Op::SQRT:       return umaker_ptr();
    case Op::SQUARE:     return umaker_ptr();

    // Math: special
    case Op::ERF:        return umaker_ptr();
    case Op::ERFC:       return umaker_ptr();
    case Op::GAMMA:      return umaker_ptr();
    case Op::LGAMMA:     return umaker_ptr();

    // Math: floating-point
    case Op::ISFINITE:   return umaker_ptr();
    case Op::ISINF:      return umaker_ptr();
    case Op::ISNA:       return umaker_ptr();
    case Op::CEIL:       return umaker_ptr();
    case Op::COPYSIGN:   return umaker_ptr();
    case Op::ABS:        return umaker_ptr();
    case Op::FABS:       return umaker_ptr();
    case Op::FLOOR:      return umaker_ptr();
    case Op::NEXTAFTER:  return umaker_ptr();
    case Op::SIGN:       return umaker_ptr();
    case Op::SIGNBIT:    return umaker_ptr();
    case Op::TRUNC:      return umaker_ptr();

    // Misc
    case Op::LEN:        return umaker_ptr();
    case Op::CLIP:       return umaker_ptr();
    case Op::MAXIMUM:    return umaker_ptr();
    case Op::MINIMUM:    return umaker_ptr();

    default: throw RuntimeError() << "Unknown unary op " << int(opcode);
  }
}




//------------------------------------------------------------------------------
// Main unaryop function
//------------------------------------------------------------------------------

Column unaryop(Op opcode, Column&& col)
{
  // Find the maker function
  auto id = make_id(opcode, col.stype());
  if (umakers_library.count(id) == 0) {
    umakers_library[id] = resolve_op(opcode, col.stype());
  }
  const umaker_ptr& maker = umakers_library[id];

  if (!maker) {  // fallback to OLD code
    const auto& info = unary_library.get_infox(opcode, col.stype());
    if (info.cast_stype != SType::VOID) {
      col.cast_inplace(info.cast_stype);
    }
    return info.vcolfn(std::move(col));
  }

  xassert(maker);
  return maker->compute(std::move(col));
}




}}  // namespace dt::expr
