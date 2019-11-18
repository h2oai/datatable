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
#include "expr/fbinary/bimaker.h"
#include "utils/assert.h"
#include "column.h"
namespace dt {
namespace expr {


//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

bimaker::~bimaker() = default;


static constexpr size_t make_id(Op opcode, SType st1, SType st2) noexcept {
  return ((static_cast<size_t>(opcode) - BINOP_FIRST) << 16) +
         (static_cast<size_t>(st1) << 8) +
         (static_cast<size_t>(st2));
}

static std::unordered_map<size_t, bimaker_ptr> bimakers_library;


bimaker_ptr resolve_op(Op opcode, SType stype1, SType stype2) {
  switch (opcode) {
    case Op::PLUS:     return resolve_op_plus(stype1, stype2);
    case Op::MINUS:    return resolve_op_minus(stype1, stype2);
    case Op::MULTIPLY: return resolve_op_multiply(stype1, stype2);
    case Op::DIVIDE:   return resolve_op_divide(stype1, stype2);
    case Op::INTDIV:
    case Op::MODULO:
    case Op::POWER:    return bimaker_ptr();
    case Op::AND:      return resolve_op_and(stype1, stype2);
    case Op::OR:       return resolve_op_or(stype1, stype2);
    case Op::XOR:      return resolve_op_xor(stype1, stype2);
    case Op::LSHIFT:   return resolve_op_lshift(stype1, stype2);
    case Op::RSHIFT:   return resolve_op_rshift(stype1, stype2);
    case Op::EQ:       return resolve_op_eq(stype1, stype2);
    case Op::NE:       return resolve_op_ne(stype1, stype2);
    case Op::LT:       return resolve_op_lt(stype1, stype2);
    case Op::GT:       return resolve_op_gt(stype1, stype2);
    case Op::LE:       return resolve_op_le(stype1, stype2);
    case Op::GE:       return resolve_op_ge(stype1, stype2);
    default: throw RuntimeError() << "Unknown binary op " << int(opcode);
  }
}




//------------------------------------------------------------------------------
// Main binaryop function
//------------------------------------------------------------------------------

Column new_binaryop(Op opcode, Column&& col1, Column&& col2)
{
  xassert(col1.nrows() == col2.nrows());
  xassert(static_cast<size_t>(opcode) >= BINOP_FIRST &&
          static_cast<size_t>(opcode) <= BINOP_LAST);

  // Find the maker function
  auto id = make_id(opcode, col1.stype(), col2.stype());
  if (bimakers_library.count(id) == 0) {
    bimakers_library[id] = resolve_op(opcode, col1.stype(), col2.stype());
  }
  const bimaker_ptr& maker = bimakers_library[id];

  // temporary: fall back to old code
  if (!maker) return binaryop(opcode, col1, col2);

  return maker->compute(std::move(col1), std::move(col2));
}




}}  // namespace dt::expr
