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
#ifndef dt_COLUMN_SHIFT_h
#define dt_COLUMN_SHIFT_h
#include <memory>
#include "column/virtual.h"
namespace dt {


/**
  * Virtual column representing the column `arg_` shifted by a
  * certain number of observation forward (if LAG is true), or
  * backward (if LAG is false).
  *
  * The observations at the beginning/end of the shifted region are
  * filled with NAs.
  */
template <bool LAG>
class Shift_ColumnImpl : public Virtual_ColumnImpl {
  private:
    Column arg_;
    size_t shift_;

  public:
    Shift_ColumnImpl(Column&& col, size_t shift, size_t nrows)
      : Virtual_ColumnImpl(nrows, col.stype()),
        arg_(std::move(col)),
        shift_(shift) {}

    ColumnImpl* clone() const override {
      return new Shift_ColumnImpl<LAG>(Column(arg_), shift_, nrows_);
    }

    bool allow_parallel_access() const override {
      return arg_.allow_parallel_access();
    }


    bool get_element(size_t i, int8_t* out)   const override { return _elem(i, out); }
    bool get_element(size_t i, int16_t* out)  const override { return _elem(i, out); }
    bool get_element(size_t i, int32_t* out)  const override { return _elem(i, out); }
    bool get_element(size_t i, int64_t* out)  const override { return _elem(i, out); }
    bool get_element(size_t i, float* out)    const override { return _elem(i, out); }
    bool get_element(size_t i, double* out)   const override { return _elem(i, out); }
    bool get_element(size_t i, CString* out)  const override { return _elem(i, out); }
    bool get_element(size_t i, py::robj* out) const override { return _elem(i, out); }

  private:
    template <typename T>
    inline bool _elem(size_t i, T* out) const {
      if (LAG) {
        if (i < shift_) return false;
        return arg_.get_element(i - shift_, out);
      } else {
        if (i >= nrows_ - shift_) return false;
        return arg_.get_element(i + shift_, out);
      }
    }
};




/**
  * Similar to `Shift_ColumnImpl`, but instead of missing values at
  * the end of the range, this column substitutes a specific "fill"
  * value.
  */
template <bool LAG>
class ShiftWithFill_ColumnImpl : public Virtual_ColumnImpl {
  private:
    Column arg_;
    Column fill_;
    size_t shift_;

  public:
    ShiftWithFill_ColumnImpl(Column&& col, Column&& fill, size_t shift,
                             size_t nrows)
      : Virtual_ColumnImpl(nrows, col.stype()),
        arg_(std::move(col)),
        fill_(std::move(fill)),
        shift_(shift)
    {
      xassert(shift_ > 0);
      xassert(arg_.stype() == fill_.stype());
      // Note: we could easily add support for `fill_` having `shift_` rows
      // as well.
      xassert(fill_.nrows() == 1);
    }

    ColumnImpl* clone() const override {
      return new ShiftWithFill_ColumnImpl<LAG>(
                  Column(arg_), Column(fill_), shift_, nrows_);
    }

    bool allow_parallel_access() const override {
      return arg_.allow_parallel_access();
    }


    bool get_element(size_t i, int8_t* out)   const override { return _elem(i, out); }
    bool get_element(size_t i, int16_t* out)  const override { return _elem(i, out); }
    bool get_element(size_t i, int32_t* out)  const override { return _elem(i, out); }
    bool get_element(size_t i, int64_t* out)  const override { return _elem(i, out); }
    bool get_element(size_t i, float* out)    const override { return _elem(i, out); }
    bool get_element(size_t i, double* out)   const override { return _elem(i, out); }
    bool get_element(size_t i, CString* out)  const override { return _elem(i, out); }
    bool get_element(size_t i, py::robj* out) const override { return _elem(i, out); }

  private:
    template <typename T>
    inline bool _elem(size_t i, T* out) {
      if (LAG) {
        return (i < shift_)? fill_.get_element(0, out)
                           : arg_.get_element(i - shift_, out);
      } else {
        return (i >= nrows_ - shift_)? fill_.get_element(0, out)
                                     : arg_.get_element(i + shift_, out);
      }
    }
};




}  // namespace dt
#endif
