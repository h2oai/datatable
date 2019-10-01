//------------------------------------------------------------------------------
// Copyright 2018-2019 H2O.ai
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
#include "models/utils.h"
#include "types.h"



// TODO: replace this class with a simple column cast

/**
 *  An abstract base class for all the column convertors.
 *  T is a destination type we are converting data to.
 */
template<typename T>
class ColumnConvertor {
  public:
    explicit ColumnConvertor(size_t n);
    virtual ~ColumnConvertor();
    virtual T operator[](size_t) const = 0;
    virtual void get_rows(std::vector<T>&, size_t, size_t, size_t) const = 0;
    size_t get_nrows() const;
    T get_min() const;
    T get_max() const;

  protected:
    T min;
    T max;
    size_t nrows;
};


/**
 *  Constructor for the base class, store rowindex and number of rows
 *  to be used when the data are accessed.
 */
template<typename T>
ColumnConvertor<T>::ColumnConvertor(size_t n)
  : nrows(n) {}


/**
 *  Virtual destructor.
 */
template<typename T>
ColumnConvertor<T>::~ColumnConvertor() {}

/**
 *  Getters for min, max and nrows.
 */
template<typename T>
T ColumnConvertor<T>::get_min() const {
  return min;
}


template<typename T>
T ColumnConvertor<T>::get_max() const {
  return max;
}


template<typename T>
size_t ColumnConvertor<T>::get_nrows() const {
  return nrows;
}


/**
 *  Template class to convert continuous columns data from type T1 to T2, where
 *  - T1 is a source data type, i.e. int8_t/int16_t/int32_t/int64_t/float/double;
 *  - T2 is a destination type, i.e. float/double;
 */
template<typename T1, typename T2>
class ColumnConvertorReal : public ColumnConvertor<T2> {
  static_assert(std::is_same<T2, float>::value || std::is_same<T2, double>::value,
                "Wrong T2 in ColumnConvertorReal");
  private:
    Column column;

  public:
    explicit ColumnConvertorReal(const Column&);
    T2 operator[](size_t) const override;
    void get_rows(std::vector<T2>&, size_t, size_t, size_t) const override;
};


/**
 *  Constructor for the derived class. Pre-calculate stats, so that it is ready
 *  for the multi-threaded ND aggregator.
 */
template<typename T1, typename T2>
ColumnConvertorReal<T1, T2>::ColumnConvertorReal(const Column& column_in)
  : ColumnConvertor<T2>(column_in.nrows()),
    column(column_in)
{
  using R = typename std::conditional<std::is_integral<T1>::value,
                                      int64_t, double>::type;
  R min, max;
  column_in.stats()->get_stat(Stat::Min, &min);
  column_in.stats()->get_stat(Stat::Max, &max);
  this->min = static_cast<T2>(min);
  this->max = static_cast<T2>(max);
}


/**
 *  operator[] to get a converted column value by
 *  - casting it to T2;
 *  - applying a rowindex;
 *  - properly handling NA's.
 */
template<typename T1, typename T2>
T2 ColumnConvertorReal<T1, T2>::operator[](size_t row) const {
  T1 value;
  bool isvalid = column.get_element(row, &value);
  return isvalid? static_cast<T2>(value) : GETNA<T2>();
}


/**
 *  This method does the same as above just for a chunk of data.
 *  No bound checks are performed.
 */
template<typename T1, typename T2>
void ColumnConvertorReal<T1, T2>::get_rows(
    std::vector<T2>& buffer, size_t start, size_t step, size_t count) const
{
  for (size_t j = 0; j < count; ++j) {
    T1 value;
    bool isvalid = column.get_element(start + j * step, &value);
    buffer[j] = isvalid? static_cast<T2>(value) : GETNA<T2>();
  }
}


#endif
