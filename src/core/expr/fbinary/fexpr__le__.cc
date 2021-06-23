//------------------------------------------------------------------------------
// Copyright 2019-2020 H2O.ai
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
#include "column/func_binary.h"
#include "expr/fbinary/fexpr__compare__.h"
namespace dt {
namespace expr {


template <typename T>
static bool op_le(ref_t<T> x, bool xvalid, ref_t<T> y, bool yvalid, int8_t* out)
{
  *out = (xvalid && yvalid && x <= y) || (!xvalid && !yvalid);
  return true;
}

template <typename T>
static Column make(Column&& a, Column&& b, dt::Type type) {
  xassert(type.can_be_read_as<T>());
  size_t nrows = a.nrows();
  a.cast_inplace(type);
  b.cast_inplace(type);
  return Column(new FuncBinary2_ColumnImpl<T, T, int8_t>(
    std::move(a), std::move(b), op_le<T>, nrows, SType::BOOL
  ));
}



std::string FExpr__le__::name() const {
  return "<=";
}

int FExpr__le__::precedence() const noexcept {
  return 6;
}


Column FExpr__le__::evaluate1(Column&& lcol, Column&& rcol) const {
  xassert(lcol.nrows() == rcol.nrows());
  auto type1 = lcol.type();
  auto type2 = rcol.type();
  auto type0 = Type::common(type1, type2);

  switch (type0.stype()) {
    case SType::VOID:
    case SType::BOOL:
    case SType::INT8:
    case SType::INT16:   type0 = dt::Type::int32();  FALLTHROUGH;
    case SType::DATE32:
    case SType::INT32:   return make<int32_t>(std::move(lcol), std::move(rcol), type0);
    case SType::TIME64:
    case SType::INT64:   return make<int64_t>(std::move(lcol), std::move(rcol), type0);
    case SType::FLOAT32: return make<float>(std::move(lcol), std::move(rcol), type0);
    case SType::FLOAT64: return make<double>(std::move(lcol), std::move(rcol), type0);
    case SType::STR32:
    case SType::STR64:   return make<CString>(std::move(lcol), std::move(rcol), type0);
    default:
        throw TypeError() << "Operator `<=` cannot be applied to "
            "columns with types `" << type1 << "` and `" << type2 << "`";
  }
}






}}  // namespace dt::expr
