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
    case Op::ROWCOUNT: return naryop_rowcount(std::move(columns));
    case Op::ROWFIRST: return naryop_rowfirstlast(std::move(columns), true);
    case Op::ROWLAST:  return naryop_rowfirstlast(std::move(columns), false);
    case Op::ROWMAX:   return naryop_rowminmax(std::move(columns), false);
    case Op::ROWMEAN:  return naryop_rowmean(std::move(columns));
    case Op::ROWMIN:   return naryop_rowminmax(std::move(columns), true);
    case Op::ROWSD:    return naryop_rowsd(std::move(columns));
    case Op::ROWSUM:   return naryop_rowsum(std::move(columns));
    default:
      throw TypeError() << "Unknown n-ary op " << static_cast<int>(opcode);
  }
}



//------------------------------------------------------------------------------
// Various helper functions
//------------------------------------------------------------------------------

SType detect_common_numeric_stype(const colvec& columns, const char* fnname)
{
  SType common_stype = SType::INT32;
  for (size_t i = 0; i < columns.size(); ++i) {
    switch (columns[i].stype()) {
      case SType::BOOL:
      case SType::INT8:
      case SType::INT16:
      case SType::INT32: break;
      case SType::INT64: {
        if (common_stype == SType::INT32) common_stype = SType::INT64;
        break;
      }
      case SType::FLOAT32: {
        if (common_stype == SType::INT32 || common_stype == SType::INT64) {
          common_stype = SType::FLOAT32;
        }
        break;
      }
      case SType::FLOAT64: {
        common_stype = SType::FLOAT64;
        break;
      }
      default:
        throw TypeError() << "Function `" << fnname << "` expects a sequence "
                             "of numeric columns, however column " << i
                          << " had type `" << columns[i].stype() << "`";
    }
  }
  #if DTDEBUG
    if (!columns.empty()) {
      size_t nrows = columns[0].nrows();
      for (const auto& col : columns) xassert(col.nrows() == nrows);
    }
  #endif
  return common_stype;
}



void promote_columns(colvec& columns, SType target_stype) {
  for (auto& col : columns) {
    col.cast_inplace(target_stype);
  }
}



}}  // namespace dt::expr
