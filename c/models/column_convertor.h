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
#ifndef dt_MODELS_COLUMN_CONVERTOR_h
#define dt_MODELS_COLUMN_CONVERTOR_h
#include "py_datatable.h"


// TODO: the only reason why we cast all the columns to `T` is that
// stats calculation is faster on real columns. When #219 gets fixed,
// we could do value casting on-the-fly, making use of the commented
// parts of the code.

/*
*  Helper template structures to convert C++ float/double types to
*  datatable STypes::FLOAT32/STypes::FLOAT64. respectively.
*/
template<typename T> struct stype {
  static void get_stype() {
    throw TypeError() << "Only float and double types are supported";
  }
};


template<> struct stype<float> {
  static SType get_stype() {
    return SType::FLOAT32;
  }
};


template<> struct stype<double> {
  static SType get_stype() {
    return SType::FLOAT64;
  }
};


/*
* An abstract base class for all the column convertors.
* T is a destination type we are converting data to.
*/
template<typename T>
class ColumnConvertor {
  public:
    const RowIndex& ri;
    explicit ColumnConvertor(const Column*);
    virtual ~ColumnConvertor();
    virtual T operator[](size_t) const = 0;
    virtual void get_rows(std::vector<T>&, size_t, size_t, size_t) const = 0;
    size_t get_nrows();
    T get_min();
    T get_max();

  protected:
    T min;
    T max;
    size_t nrows;
};


/*
* Constructor for the base class, store rowindex and number of rows
* to be used when the data are accessed.
*/
template<typename T>
ColumnConvertor<T>::ColumnConvertor(const Column* col) :
  ri(col->rowindex()),
  nrows(col->nrows)
{}


/*
* Virtual destructor.
*/
template<typename T>
ColumnConvertor<T>::~ColumnConvertor() {}

/*
* Getters for min, max and nrows.
*/
template<typename T>
T ColumnConvertor<T>::get_min() {
  return min;
}


template<typename T>
T ColumnConvertor<T>::get_max() {
  return max;
}


template<typename T>
size_t ColumnConvertor<T>::get_nrows() {
  return nrows;
}


/*
* Template class to convert continuous columns data from type T1 to T2, where
* - T1 is a source data type, i.e. int8_t/int16_t/int32_t/int64_t/float/double;
* - T2 is a destination type, i.e. float/double;
* - T3 is a column type, i.e. BoolColumn/IntColumn<*>/RealColumn<*>.
*/
template<typename T1, typename T2, typename T3>
class ColumnConvertorReal : public ColumnConvertor<T2> {
  private:
    const /* T1* */ T2* values;
    colptr column;
  public:
    explicit ColumnConvertorReal(const Column*);
    T2 operator[](size_t) const override;
    void get_rows(std::vector<T2>&, size_t, size_t, size_t) const override;
};


/*
* Constructor for the derived class. Pre-calculate stats, so that it is ready
* for the multi-threaded ND aggregator.
*/
template<typename T1, typename T2, typename T3>
ColumnConvertorReal<T1, T2, T3>::ColumnConvertorReal(const Column* column_in) :
  ColumnConvertor<T2>(column_in)
{
  SType to_stype = stype<T2>::get_stype();
  column = colptr(column_in->cast(to_stype));
  auto column_real = static_cast<RealColumn<T2>*>(column.get());
  this->min = column_real->min();
  this->max = column_real->max();
  values = column_real->elements_r();
  // auto columnT = static_cast<const T3*>(column_in);
  // this->min = static_cast<T2>(columnT->min());
  // this->max = static_cast<T2>(columnT->max());
  // values = static_cast<const T1*>(column_in->data());
}


/*
* operator[] to get a converted column value by
* - casting it to T2;
* - applying a rowindex;
* - properly handling NA's.
*/
template<typename T1, typename T2, typename T3>
T2 ColumnConvertorReal<T1, T2, T3>::operator[](size_t row) const {
  size_t i = row;
  // size_t i = this->ri[row];
  if (i == RowIndex::NA /* || ISNA<T1>(values[i]) */) {
    return GETNA<T2>();
  } else {
    return values[i];
    // return static_cast<T2>(values[i]);
  }
}


/*
* This method does the same as above just for a chunk of data.
* No bound checks are performed.
*/
template<typename T1, typename T2, typename T3>
void ColumnConvertorReal<T1, T2, T3>::get_rows(std::vector<T2>& buffer,
                                               size_t from,
                                               size_t step,
                                               size_t count) const {
  for (size_t j = 0; j < count; ++j) {
    size_t i = from + j * step;
    // size_t i = this->ri[from + j * step];
    if (i == RowIndex::NA /*|| ISNA<T1>(values[i])*/) {
      buffer[j] = GETNA<T2>();
    } else {
      buffer[j] = values[i];
      // buffer[j] = static_cast<T2>(values[i]);
    }
  }
}


#endif
