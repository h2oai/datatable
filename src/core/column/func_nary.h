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
#ifndef dt_COLUMN_FUNC_NARY_h
#define dt_COLUMN_FUNC_NARY_h
#include "column/virtual.h"
#include "utils/assert.h"
namespace dt {

using colvec = std::vector<Column>;


/**
  * Virtual function that implement an n-ary operation over a set of
  * columns. This is used in "rowwise" exprs.
  */
template <typename T>
class FuncNary_ColumnImpl : public Virtual_ColumnImpl
{
  using func_t = bool(*)(size_t, T*, const colvec&);
  private:
    colvec columns_;
    func_t evaluator_;

  public:
    FuncNary_ColumnImpl(colvec&&, func_t fn, size_t nrows, SType stype);

    ColumnImpl* clone() const override;
    void verify_integrity() const override;
    bool allow_parallel_access() const override;

    bool get_element(size_t i, T* out) const override;
};




//------------------------------------------------------------------------------
// Implementation
//------------------------------------------------------------------------------

template <typename T>
FuncNary_ColumnImpl<T>::FuncNary_ColumnImpl(
    colvec&& cols, func_t fn, size_t nrows, SType stype
)
  : Virtual_ColumnImpl(nrows, stype),
    columns_(cols),
    evaluator_(fn)
{
  assert_compatible_type<T>(stype);
}


template <typename T>
ColumnImpl* FuncNary_ColumnImpl<T>::clone() const {
  return new FuncNary_ColumnImpl<T>(
                colvec(columns_), evaluator_, nrows_, stype_);
}


template <typename T>
void FuncNary_ColumnImpl<T>::verify_integrity() const {
  XAssert(evaluator_);
  for (const auto& col : columns_) {
    XAssert(col.nrows() >= nrows_);
    col.verify_integrity();
  }
}


template <typename T>
bool FuncNary_ColumnImpl<T>::allow_parallel_access() const {
  for (const auto& col : columns_) {
    if (!col.allow_parallel_access()) return false;
  }
  return true;
}


template <typename T>
bool FuncNary_ColumnImpl<T>::get_element(size_t i, T* out) const {
  return evaluator_(i, out, columns_);
}




}  // namespace dt
#endif
