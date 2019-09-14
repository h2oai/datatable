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
    ConstNa_ColumnImpl(size_t nrows)
      : Const_ColumnImpl(nrows, SType::VOID) {}

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

class ConstInt_ColumnImpl : public ColumnImpl {
  private:
    int64_t value;

  public:
    ConstInt_ColumnImpl(size_t nrows, bool x)
      : ColumnImpl(nrows, SType::BOOL), value(x) {}

    ConstInt_ColumnImpl(size_t nrows, int64_t x)
      : ColumnImpl(nrows, stype_for_value(x)), value(x) {}

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

class ConstFloat_ColumnImpl : public ColumnImpl {
  private:
    double value;

  public:
    ConstFloat_ColumnImpl(size_t nrows, double x)
      : ColumnImpl(nrows, SType::FLOAT64), value(x) {}

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

class ConstString_ColumnImpl : public ColumnImpl {
  private:
    CString value;

  public:
    ConstString_ColumnImpl(size_t nrows, CString x)
      : ColumnImpl(nrows, SType::STR32), value(x) {}

    ColumnImpl* shallowcopy() const override {
      return new ConstString_ColumnImpl(_nrows, value);
    }

    bool get_element(size_t, CString* out) const override {
      *out = value;
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


bool Const_ColumnImpl::is_virtual() const noexcept {
  return true;
}




}  // namespace dt
