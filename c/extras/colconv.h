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
#ifndef C_EXTRAS_COLCONV_H_
#define C_EXTRAS_COLCONV_H_
#include "py_datatable.h"

/*
* An abstract base class for all the column convertors.
*/
class ColumnConvertor {
  public:
    explicit ColumnConvertor(const Column*);
    virtual ~ColumnConvertor();

    const RowIndex& ri;
    virtual float get_value(size_t row) const = 0;
};


/*
* Template class to hash integers.
* T1 is a data type, i.e. int8_t/int16_t/int32_t/int64_t/float/double
* T2 is a destination type, i.e. float/double
*/
template<typename T1, typename T2>
class ColumnConvertorT : public ColumnConvertor {
  private:
    const T1* values;
  public:
    explicit ColumnConvertorT(const Column*);
    T2 get_value(size_t) const override;
};

template<typename T1, typename T2>
ColumnConvertorT<T1, T2>::ColumnConvertorT(const Column* col) : ColumnConvertor(col) {
  values = static_cast<const T1*>(col->data());
}

template<typename T1, typename T2>
T2 ColumnConvertorT<T1, T2>::get_value(size_t row) const {
  T1 value = values[row];
  if (ISNA<T1>(value)) return GETNA<T2>();
  else return static_cast<T2>(values[row]);
}

#endif /* C_EXTRAS_COLCONV_H_ */
