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
#include "expr/fnary/fnary.h"
#include "utils/exceptions.h"
#include "column.h"
namespace dt {
namespace expr {



Column naryop(Op opcode, colvec&& columns) {
  switch (opcode) {
    case Op::ROWALL:   return naryop_rowall(std::move(columns));
    case Op::ROWANY:   return naryop_rowany(std::move(columns));
    case Op::ROWCOUNT:
    case Op::ROWFIRST:
    case Op::ROWLAST:
    case Op::ROWMAX:
    case Op::ROWMEAN:
    case Op::ROWMIN:
    case Op::ROWSD:
    case Op::ROWSUM:
    default:
      throw TypeError() << "Unknown n-ary op " << static_cast<int>(opcode);
  }
}




}}  // namespace dt::expr
