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
#include "column/const.h"
#include "python/obj.h"
#include "utils/macros.h"
namespace dt {


//------------------------------------------------------------------------------
// ConstFloat_ColumnImpl
//------------------------------------------------------------------------------

class ConstFloat_ColumnImpl : public Const_ColumnImpl {
  private:
    double value;

  public:
    ConstFloat_ColumnImpl(size_t nrows, float x, SType stype = SType::FLOAT32)
      : Const_ColumnImpl(nrows, stype),
        value(static_cast<double>(x))
    {
      xassert(stype == SType::FLOAT32 || stype == SType::FLOAT64);
    }

    ConstFloat_ColumnImpl(size_t nrows, double x, SType stype = SType::FLOAT64)
      : Const_ColumnImpl(nrows, normalize_stype(stype, x)),
        value(x) {}

    ColumnImpl* clone() const override {
      return new ConstFloat_ColumnImpl(nrows_, value, stype_);
    }

    bool get_element(size_t, float* out) const override {
      *out = static_cast<float>(value);
      return true;
    }

    bool get_element(size_t, double* out) const override {
      *out = value;
      return true;
    }


  private:
    static SType normalize_stype(SType stype0, double x) {
      constexpr double MAXF32 = double(std::numeric_limits<float>::max());
      switch (stype0) {
        case SType::FLOAT32:
          if (std::abs(x) <= MAXF32) return SType::FLOAT32;
          FALLTHROUGH;

        case SType::FLOAT64:
        case SType::VOID:
          return SType::FLOAT64;

        default:
          throw RuntimeError() << "Wrong `stype0` in `normalize_stype()`: "
                  << stype0;  // LCOV_EXCL_LINE
      }
    }
};




//------------------------------------------------------------------------------
// ConstString_ColumnImpl
//------------------------------------------------------------------------------

class ConstString_ColumnImpl : public Const_ColumnImpl {
  private:
    std::string value;

  public:
    ConstString_ColumnImpl(size_t nrows, const CString& x)
      : Const_ColumnImpl(nrows, SType::STR32),
        value(x.to_string()) {}

    ConstString_ColumnImpl(size_t nrows, const CString& x, SType stype)
      : Const_ColumnImpl(nrows, stype),
        value(x.to_string())
    {
      xassert(stype == SType::STR32 || stype == SType::STR64);
    }

    ConstString_ColumnImpl(size_t nrows, std::string x)
      : Const_ColumnImpl(nrows, SType::STR32),
        value(std::move(x)) {}

    ColumnImpl* clone() const override {
      return new ConstString_ColumnImpl(nrows_, CString(value), stype_);
    }

    bool get_element(size_t, CString* out) const override {
      *out = CString(value);
      return true;
    }
};




//------------------------------------------------------------------------------
// Const_ColumnImpl
//------------------------------------------------------------------------------

Column Const_ColumnImpl::make_na_column(size_t nrows) {
  return Column(new ConstNa_ColumnImpl(nrows));
}

Column Const_ColumnImpl::make_bool_column(size_t nrows, bool x) {
  return Column(new ConstInt_ColumnImpl(nrows, x));
}

Column Const_ColumnImpl::make_int_column(size_t nrows, int64_t x, SType st) {
  return Column(new ConstInt_ColumnImpl(nrows, x, st));
}

Column Const_ColumnImpl::make_float_column(size_t nrows, double x, SType st) {
  return Column(new ConstFloat_ColumnImpl(nrows, x, st));
}

Column Const_ColumnImpl::make_string_column(size_t nrows, const CString& x, SType st) {
  return Column(new ConstString_ColumnImpl(nrows, x, st));
}


template <typename COL, SType stype>
static Column _make(const Column& col) {
  read_t<stype> value;
  bool isvalid = col.get_element(0, &value);
  return isvalid? Column(new COL(1, value, stype))
                : Column(new ConstNa_ColumnImpl(1, stype));
}

Column Const_ColumnImpl::from_1row_column(const Column& col) {
  xassert(col.nrows() == 1);
  switch (col.stype()) {
    case SType::BOOL:    return _make<ConstInt_ColumnImpl, SType::BOOL>(col);
    case SType::INT8:    return _make<ConstInt_ColumnImpl, SType::INT8>(col);
    case SType::INT16:   return _make<ConstInt_ColumnImpl, SType::INT16>(col);
    case SType::INT32:   return _make<ConstInt_ColumnImpl, SType::INT32>(col);
    case SType::INT64:   return _make<ConstInt_ColumnImpl, SType::INT64>(col);
    case SType::FLOAT32: return _make<ConstFloat_ColumnImpl, SType::FLOAT32>(col);
    case SType::FLOAT64: return _make<ConstFloat_ColumnImpl, SType::FLOAT64>(col);
    case SType::STR32:   return _make<ConstString_ColumnImpl, SType::STR32>(col);
    case SType::STR64:   return _make<ConstString_ColumnImpl, SType::STR64>(col);
    default:
      throw NotImplError() << "Cannot convert 1-row column of stype "
                           << col.stype();
  }
}


size_t Const_ColumnImpl::n_children() const noexcept {
  return 0;
}

void Const_ColumnImpl::repeat(size_t ntimes, Column&) {
  nrows_ *= ntimes;
}




}  // namespace dt
