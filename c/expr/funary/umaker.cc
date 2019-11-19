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


umaker_ptr resolve_op(Op opcode, SType stype) {
  (void) stype;
  switch (opcode) {
    case Op::UPLUS:      return umaker_ptr();
    case Op::UMINUS:     return umaker_ptr();
    case Op::UINVERT:    return umaker_ptr();
    case Op::ISFINITE:   return umaker_ptr();
    case Op::ISINF:      return umaker_ptr();
    case Op::ISNA:       return umaker_ptr();
    case Op::ABS:        return umaker_ptr();
    case Op::LEN:        return umaker_ptr();

    // Math: trigonometric
    case Op::SIN:
    case Op::COS:
    case Op::TAN:
    case Op::ARCSIN:
    case Op::ARCCOS:
    case Op::ARCTAN:
    case Op::ARCTAN2:
    case Op::HYPOT:
    case Op::DEG2RAD:
    case Op::RAD2DEG:

    // Math: hyperbolic
    case Op::SINH:
    case Op::COSH:
    case Op::TANH:
    case Op::ARSINH:
    case Op::ARCOSH:
    case Op::ARTANH:

    // Math: exponential/power
    case Op::CBRT:
    case Op::EXP:
    case Op::EXP2:
    case Op::EXPM1:
    case Op::LOG:
    case Op::LOG10:
    case Op::LOG1P:
    case Op::LOG2:
    case Op::LOGADDEXP:
    case Op::LOGADDEXP2:
    case Op::SQRT:
    case Op::SQUARE:

    // Math: special
    case Op::ERF:
    case Op::ERFC:
    case Op::GAMMA:
    case Op::LGAMMA:

    // Math: floating-point
    case Op::CEIL:
    case Op::COPYSIGN:
    case Op::FABS:
    case Op::FLOOR:
    case Op::NEXTAFTER:
    case Op::SIGN:
    case Op::SIGNBIT:
    case Op::TRUNC:

    // Math: misc
    case Op::CLIP:
    case Op::MAXIMUM:
    case Op::MINIMUM:   return umaker_ptr();

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
