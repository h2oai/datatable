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
#include "column/const.h"
#include "python/obj.h"
namespace dt {



//------------------------------------------------------------------------------
// ConstNa_ColumnImpl
//------------------------------------------------------------------------------

class ConstNa_ColumnImpl : public Const_ColumnImpl {
  public:
    ConstNa_ColumnImpl(size_t nrows, SType stype = SType::VOID)
      : Const_ColumnImpl(nrows, stype) {}

    bool get_element(size_t, int8_t*)   const override { return true; }
    bool get_element(size_t, int16_t*)  const override { return true; }
    bool get_element(size_t, int32_t*)  const override { return true; }
    bool get_element(size_t, int64_t*)  const override { return true; }
    bool get_element(size_t, float*)    const override { return true; }
    bool get_element(size_t, double*)   const override { return true; }
    bool get_element(size_t, CString*)  const override { return true; }
    bool get_element(size_t, py::robj*) const override { return true; }

    // TODO: override cast()

    ColumnImpl* shallowcopy() const override {
      return new ConstNa_ColumnImpl(_nrows);
    }

    // VOID column materializes into BOOL stype
    ColumnImpl* materialize() override {
      ColumnImpl* out = ColumnImpl::new_impl(SType::BOOL);
      out->_nrows = _nrows;
      out->init_data();
      out->fill_na();
      return out;
    }

    void resize_and_fill(size_t nrows) override {
      _nrows = nrows;
    }
};




//------------------------------------------------------------------------------
// ConstInt_ColumnImpl
//------------------------------------------------------------------------------

class ConstInt_ColumnImpl : public Const_ColumnImpl {
  private:
    int64_t value;

  public:
    ConstInt_ColumnImpl(size_t nrows, bool x)
      : Const_ColumnImpl(nrows, SType::BOOL), value(x) {}

    ConstInt_ColumnImpl(size_t nrows, int64_t x)
      : Const_ColumnImpl(nrows, stype_for_value(x)), value(x) {}

    ConstInt_ColumnImpl(size_t nrows, int8_t x, SType stype)
      : Const_ColumnImpl(nrows, stype), value(x) {}

    ConstInt_ColumnImpl(size_t nrows, int16_t x, SType stype)
      : Const_ColumnImpl(nrows, stype), value(x) {}

    ConstInt_ColumnImpl(size_t nrows, int32_t x, SType stype)
      : Const_ColumnImpl(nrows, stype), value(x) {}

    ConstInt_ColumnImpl(size_t nrows, int64_t x, SType stype)
      : Const_ColumnImpl(nrows, stype), value(x) {}

    ColumnImpl* shallowcopy() const override {
      return new ConstInt_ColumnImpl(_nrows, value);
    }

    bool get_element(size_t, int8_t* out) const override {
      *out = static_cast<int8_t>(value);
      return false;
    }

    bool get_element(size_t, int16_t* out) const override {
      *out = static_cast<int16_t>(value);
      return false;
    }

    bool get_element(size_t, int32_t* out) const override {
      *out = static_cast<int32_t>(value);
      return false;
    }

    bool get_element(size_t, int64_t* out) const override {
      *out = value;
      return false;
    }

    bool get_element(size_t, float* out) const override {
      *out = static_cast<float>(value);
      return false;
    }

    bool get_element(size_t, double* out) const override {
      *out = static_cast<double>(value);
      return false;
    }


  private:
    static SType stype_for_value(int64_t x) {
      return (x == static_cast<int32_t>(x))? SType::INT32 : SType::INT64;
    }
};




//------------------------------------------------------------------------------
// ConstFloat_ColumnImpl
//------------------------------------------------------------------------------

class ConstFloat_ColumnImpl : public Const_ColumnImpl {
  private:
    double value;

  public:
    ConstFloat_ColumnImpl(size_t nrows, double x)
      : Const_ColumnImpl(nrows, SType::FLOAT64), value(x) {}

    ConstFloat_ColumnImpl(size_t nrows, float x, SType stype)
      : Const_ColumnImpl(nrows, stype), value(static_cast<double>(x)) {}

    ConstFloat_ColumnImpl(size_t nrows, double x, SType stype)
      : Const_ColumnImpl(nrows, stype), value(x) {}

    ColumnImpl* shallowcopy() const override {
      return new ConstFloat_ColumnImpl(_nrows, value);
    }

    bool get_element(size_t, float* out) const override {
      *out = static_cast<float>(value);
      return false;
    }

    bool get_element(size_t, double* out) const override {
      *out = value;
      return false;
    }
};




//------------------------------------------------------------------------------
// ConstString_ColumnImpl
//------------------------------------------------------------------------------

class ConstString_ColumnImpl : public Const_ColumnImpl {
  private:
    std::string value;

  public:
    ConstString_ColumnImpl(size_t nrows, CString x)
      : Const_ColumnImpl(nrows, SType::STR32),
        value(x.ch, static_cast<size_t>(x.size)) {}

    ConstString_ColumnImpl(size_t nrows, CString x, SType stype)
      : Const_ColumnImpl(nrows, stype),
        value(x.ch, static_cast<size_t>(x.size)) {}

    ConstString_ColumnImpl(size_t nrows, std::string x)
      : Const_ColumnImpl(nrows, SType::STR32),
        value(std::move(x)) {}

    ColumnImpl* shallowcopy() const override {
      return new ConstString_ColumnImpl(_nrows, value);
    }

    bool get_element(size_t, CString* out) const override {
      out->ch = value.c_str();
      out->size = static_cast<int64_t>(value.size());
      return false;
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

Column Const_ColumnImpl::make_int_column(size_t nrows, int64_t x) {
  return Column(new ConstInt_ColumnImpl(nrows, x));
}

Column Const_ColumnImpl::make_float_column(size_t nrows, double x) {
  return Column(new ConstFloat_ColumnImpl(nrows, x));
}

Column Const_ColumnImpl::make_string_column(size_t nrows, CString x) {
  return Column(new ConstString_ColumnImpl(nrows, x));
}


template <typename COL, SType stype>
static Column _make(const Column& col) {
  read_t<stype> value;
  bool isna = col.get_element(0, &value);
  return isna? Column(new ConstNa_ColumnImpl(1, stype))
             : Column(new COL(1, value, stype));
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



bool Const_ColumnImpl::is_virtual() const noexcept {
  return true;
}


void Const_ColumnImpl::repeat(size_t ntimes, bool inplace, Column& out) {
  if (inplace) {
    _nrows *= ntimes;
  }
  else {
    ColumnImpl* newimpl = this->shallowcopy();
    newimpl->repeat(ntimes, true, out);
    out = Column(newimpl);
  }
}




}  // namespace dt
