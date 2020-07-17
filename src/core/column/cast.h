//------------------------------------------------------------------------------
// Copyright 2020 H2O.ai
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
#ifndef dt_COLUMN_CAST_h
#define dt_COLUMN_CAST_h
#include "column/virtual.h"
namespace dt {


//------------------------------------------------------------------------------
// Base class used for other Cast* columns
//------------------------------------------------------------------------------

class Cast_ColumnImpl : public Virtual_ColumnImpl {
  protected:
    Column arg_;

  public:
    Cast_ColumnImpl(SType new_stype, Column&& col);

    size_t n_children() const noexcept override;
    const Column& child(size_t) const override;
};




//------------------------------------------------------------------------------
// CastBool_ColumnImpl
//------------------------------------------------------------------------------

/**
  * Virtual column that casts a boolean column `arg_` into
  * the target stype.
  */
class CastBool_ColumnImpl : public Cast_ColumnImpl {
  public:
    using Cast_ColumnImpl::Cast_ColumnImpl;

    ColumnImpl* clone() const override;

    bool get_element(size_t, int8_t*)   const override;
    bool get_element(size_t, int16_t*)  const override;
    bool get_element(size_t, int32_t*)  const override;
    bool get_element(size_t, int64_t*)  const override;
    bool get_element(size_t, float*)    const override;
    bool get_element(size_t, double*)   const override;
    bool get_element(size_t, CString*)  const override;
    bool get_element(size_t, py::oobj*) const override;

  private:
    template <typename T> inline bool _get(size_t i, T* out) const;
};




//------------------------------------------------------------------------------
// CastNumericToBool_ColumnImpl
//------------------------------------------------------------------------------

template <typename T>
class CastNumericToBool_ColumnImpl : public Cast_ColumnImpl {
  public:
    CastNumericToBool_ColumnImpl(Column&&);

    ColumnImpl* clone() const override;

    bool get_element(size_t, int8_t*) const override;
};

extern template class CastNumericToBool_ColumnImpl<int8_t>;
extern template class CastNumericToBool_ColumnImpl<int16_t>;
extern template class CastNumericToBool_ColumnImpl<int32_t>;
extern template class CastNumericToBool_ColumnImpl<int64_t>;
extern template class CastNumericToBool_ColumnImpl<float>;
extern template class CastNumericToBool_ColumnImpl<double>;




//------------------------------------------------------------------------------
// CastInt_ColumnImpl
//------------------------------------------------------------------------------

template <typename T>
class CastInt_ColumnImpl : public Cast_ColumnImpl {
  public:
    using Cast_ColumnImpl::arg_;
    using Cast_ColumnImpl::Cast_ColumnImpl;

    ColumnImpl* clone() const override;

    bool get_element(size_t, int8_t*)   const override;
    bool get_element(size_t, int16_t*)  const override;
    bool get_element(size_t, int32_t*)  const override;
    bool get_element(size_t, int64_t*)  const override;
    bool get_element(size_t, float*)    const override;
    bool get_element(size_t, double*)   const override;
    bool get_element(size_t, CString*)  const override;
    bool get_element(size_t, py::oobj*) const override;

  private:
    template <typename V> inline bool _get(size_t i, V* out) const;
};


extern template class CastInt_ColumnImpl<int8_t>;
extern template class CastInt_ColumnImpl<int16_t>;
extern template class CastInt_ColumnImpl<int32_t>;
extern template class CastInt_ColumnImpl<int64_t>;



}  // namespace dt
#endif
