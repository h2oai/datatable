//------------------------------------------------------------------------------
// Copyright 2018 H2O.ai
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
#ifndef C_EXTRAS_COLUMN_CONVERTOR_H_
#define C_EXTRAS_COLUMN_CONVERTOR_H_
#include "py_datatable.h"

/*
* An abstract base class for all the column convertors.
* T is a destination type we are converting data to.
*/
template<typename T>
class ColumnConvertor {
  public:
    explicit ColumnConvertor(const Column*);
    virtual ~ColumnConvertor();
    virtual T operator[](size_t) const = 0;
    const RowIndex& ri;
    T min;
    T max;
    size_t nrows;
};

template<typename T>
ColumnConvertor<T>::ColumnConvertor(const Column* col) : ri(col->rowindex()), nrows(col->nrows) {}
template<typename T>
ColumnConvertor<T>::~ColumnConvertor() {}


/*
* Template class to convert numeric types.
* T1 is a source data type, i.e. int8_t/int16_t/int32_t/int64_t/float/double.
* T2 is a destination type, i.e. float/double
*/
template<typename T1, typename T2, typename T3>
class ColumnConvertorContinuous : public ColumnConvertor<T2> {
  private:
    const T1* values;
  public:
    explicit ColumnConvertorContinuous(const Column*);
    T2 operator[](size_t) const override;
};

template<typename T1, typename T2, typename T3>
ColumnConvertorContinuous<T1, T2, T3>::ColumnConvertorContinuous(const Column* col) : ColumnConvertor<T2>(col) {
  auto colT = static_cast<const T3*>(col);
  this->min = static_cast<T2>(colT->min());
  this->max = static_cast<T2>(colT->max());
  values = static_cast<const T1*>(col->data());
}

template<typename T1, typename T2, typename T3>
T2 ColumnConvertorContinuous<T1, T2, T3>::operator[](size_t row) const {
  size_t i = this->ri[row];
  if (i == RowIndex::NA || ISNA<T1>(values[i])) {
    return GETNA<T2>();
  } else {
    return static_cast<T2>(values[i]);
  }
}

#endif /* C_EXTRAS_COLUMN_CONVERTOR_H_ */
