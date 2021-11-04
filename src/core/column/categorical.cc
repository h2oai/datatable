//------------------------------------------------------------------------------
// Copyright 2021 H2O.ai
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
#include "column/categorical.h"

namespace dt {


template <typename T>
Type _type_from_cattype(const Type& tcat) {
  Type t;
  switch (sizeof(T)) {
    case 1: t = Type::cat8(tcat); break;
    case 2: t = Type::cat16(tcat); break;
    case 4: t = Type::cat32(tcat); break;
    default : throw RuntimeError() << "Type is not supported";
  }
  return t;
}


template <typename T>
Categorical_ColumnImpl<T>::Categorical_ColumnImpl(
    size_t nrows, Buffer&& codes, Column&& categories
) : Virtual_ColumnImpl(nrows, _type_from_cattype<T>(categories.type())),
    codes_(std::move(codes)),
    categories_(std::move(categories))
{
  xassert(codes_.size() >= sizeof(T) * nrows);
}


template <typename T>
void Categorical_ColumnImpl<T>::materialize(Column&, bool) {
  categories_.materialize();
}


template <typename T>
ColumnImpl* Categorical_ColumnImpl<T>::clone() const {
  return
    new Categorical_ColumnImpl(nrows_, Buffer(codes_), Column(categories_));
}


template <typename T>
size_t Categorical_ColumnImpl<T>::n_children() const noexcept {
  return 1;
}


template <typename T>
const Column& Categorical_ColumnImpl<T>::child(size_t i) const {
  xassert(i == 0); (void) i;
  return categories_;
}


template <typename T>
size_t Categorical_ColumnImpl<T>::num_buffers() const noexcept {
  return 1;
}


template <typename T>
Buffer Categorical_ColumnImpl<T>::get_buffer() const noexcept {
  return codes_;
}


template <typename T>
template <typename U>
bool Categorical_ColumnImpl<T>::get_element_(size_t i, U* out) const {
  xassert(i < nrows_);
  size_t ii = static_cast<size_t>(codes_.get_element<T>(i));
  bool valid = categories_.get_element(ii, out);
  return valid;
}


template <typename T>
bool Categorical_ColumnImpl<T>::get_element(size_t i, int8_t* out) const {
  return get_element_(i, out);
}


template <typename T>
bool Categorical_ColumnImpl<T>::get_element(size_t i, int16_t* out) const {
  return get_element_(i, out);
}


template <typename T>
bool Categorical_ColumnImpl<T>::get_element(size_t i, int32_t* out) const {
  return get_element_(i, out);
}


template <typename T>
bool Categorical_ColumnImpl<T>::get_element(size_t i, int64_t* out) const {
  return get_element_(i, out);
}


template <typename T>
bool Categorical_ColumnImpl<T>::get_element(size_t i, float* out) const {
  return get_element_(i, out);
}


template <typename T>
bool Categorical_ColumnImpl<T>::get_element(size_t i, double* out) const {
  return get_element_(i, out);
}


template <typename T>
bool Categorical_ColumnImpl<T>::get_element(size_t i, CString* out) const {
  return get_element_(i, out);
}


template <typename T>
bool Categorical_ColumnImpl<T>::get_element(size_t i, py::oobj* out) const {
  return get_element_(i, out);
}


template <typename T>
bool Categorical_ColumnImpl<T>::get_element(size_t i, Column* out) const {
  return get_element_(i, out);
}


template class Categorical_ColumnImpl<uint8_t>;
template class Categorical_ColumnImpl<uint16_t>;
template class Categorical_ColumnImpl<uint32_t>;


}  // namespace dt
