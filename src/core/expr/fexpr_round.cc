//------------------------------------------------------------------------------
// Copyright 2020-2021 H2O.ai
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
#include <type_traits>
#include <limits>
#include "_dt.h"
#include "column/const.h"
#include "column/virtual.h"
#include "documentation.h"
#include "expr/eval_context.h"
#include "expr/fexpr_func_unary.h"
#include "python/xargs.h"
namespace dt {
namespace expr {

static constexpr int NODIGITS = std::numeric_limits<int>::min();


//------------------------------------------------------------------------------
// RoundNeg_ColumnImpl
//------------------------------------------------------------------------------

/**
  * Virtual column that implements rounding to a negative `ndigits`.
  * The parameter `scale_` should be equal to `10 ** |ndigits|`.
  *
  * In order to perform rounding this function applies the transform
  *
  *     std::rint(value / scale) * scale
  *
  * where `std::rint()` rounds to the nearest integer. For example,
  * when `ndigits=-1`, then `scale` is 10 and rounding 12345 yields
  * `rint(1234.5) * 10 = 12340`.
  *
  * We use floating-point arithmetics even when dealing with integer
  * values because of the way C implements integer division of
  * negatives: truncation towards 0 instead of flooring. Also,
  * rounding towards the nearest even integer is not trivial either.
  *
  * The stype of this column is always the same as the stype of its
  * argument `arg_`.
  */
template <typename T>
class RoundNeg_ColumnImpl : public Virtual_ColumnImpl {
  private:
    Column arg_;
    double scale_;

  public:
    RoundNeg_ColumnImpl(Column&& arg, double scale)
      : Virtual_ColumnImpl(arg.nrows(), arg.stype()),
        arg_(std::move(arg)),
        scale_(scale)
    {
      xassert(arg_.can_be_read_as<T>());
    }

    ColumnImpl* clone() const override {
      return new RoundNeg_ColumnImpl(Column(arg_), scale_);
    }

    size_t n_children() const noexcept override { return 1; }
    const Column& child(size_t) const override { return arg_; }

    bool get_element(size_t i, T* out) const override {
      T value;
      bool isvalid = arg_.get_element(i, &value);
      if (isvalid) {
        *out = static_cast<T>(
                std::rint(static_cast<double>(value) / scale_) * scale_);
      }
      return isvalid;
    }
};



//------------------------------------------------------------------------------
// RoundPos_ColumnImpl
//------------------------------------------------------------------------------

/**
  * Virtual column that implements rounding to a positive `ndigits`.
  * The parameter `scale_` should be equal to `10 ** ndigits`.
  *
  * In order to perform rounding this function applies the transform
  *
  *     std::rint(value * scale) / scale
  *
  * For example, when `ndigits=1`, then `scale` is 10 and rounding
  * 12.345 yields `rint(123.45) / 10 = 12.3`.
  *
  * The stype of this column is always the same as the stype of its
  * argument `arg_`. This virtual column is used for floating-point
  * columns only.
  */
template <typename T>
class RoundPos_ColumnImpl : public Virtual_ColumnImpl {
  static_assert(std::is_floating_point<T>::value, "");
  private:
    Column arg_;
    double scale_;

  public:
    RoundPos_ColumnImpl(Column&& arg, double scale)
      : Virtual_ColumnImpl(arg.nrows(), arg.stype()),
        arg_(std::move(arg)),
        scale_(scale)
    {
      xassert(compatible_type<T>(arg_.stype()));
    }

    ColumnImpl* clone() const override {
      return new RoundPos_ColumnImpl(Column(arg_), scale_);
    }

    size_t n_children() const noexcept override { return 1; }
    const Column& child(size_t) const override { return arg_; }

    bool get_element(size_t i, T* out) const override {
      T value;
      bool isvalid = arg_.get_element(i, &value);
      if (isvalid) {
        *out = static_cast<T>(
                std::rint(static_cast<double>(value) * scale_) / scale_);
      }
      return isvalid;
    }
};



//------------------------------------------------------------------------------
// Round_ColumnImpl
//------------------------------------------------------------------------------

/**
  * Virtual column that implements rounding towards 0 digits.
  * Unlike the previous classes, it allows the type of the output
  * to be different from the type of the input, accommodating both
  * the case `ndigits=0` and `ndigits=None`.
  */
template <typename TI, typename TO>
class Round_ColumnImpl : public Virtual_ColumnImpl {
  static_assert(std::is_floating_point<TI>::value, "");
  static_assert(std::is_same<TO, TI>::value ||
                std::is_same<TO, int64_t>::value, "");
  private:
    Column arg_;

  public:
    Round_ColumnImpl(Column&& arg, SType out_stype)
      : Virtual_ColumnImpl(arg.nrows(), out_stype),
        arg_(std::move(arg))
    {
      xassert(compatible_type<TI>(arg_.stype()));
      xassert(compatible_type<TO>(out_stype));
    }

    ColumnImpl* clone() const override {
      return new Round_ColumnImpl(Column(arg_), stype());
    }

    size_t n_children() const noexcept override { return 1; }
    const Column& child(size_t) const override { return arg_; }

    bool get_element(size_t i, TO* out) const override {
      TI value;
      bool isvalid = arg_.get_element(i, &value);
      if (isvalid) {
        *out = static_cast<TO>(std::rint(value));
      }
      return isvalid;
    }
};



//------------------------------------------------------------------------------
// FExpr_Round
//------------------------------------------------------------------------------

class FExpr_Round : public FExpr_FuncUnary {
  private:
    int ndigits_;
    int : 32;

  public:
    FExpr_Round(ptrExpr&& arg, int ndigits)
      : FExpr_FuncUnary(std::move(arg)),
        ndigits_(ndigits)
    {}

    std::string name() const override {
      return "round";
    }

    std::string repr() const override {
      std::string out = FExpr_FuncUnary::repr();
      if (ndigits_ != NODIGITS) {
        out.erase(out.size() - 1);  // remove ')'
        out += ", ndigits=";
        out += std::to_string(ndigits_);
        out += ")";
      }
      return out;
    }


    Column evaluate1(Column&& col) const override {
      switch (col.stype()) {
        case SType::VOID:    return std::move(col);
        case SType::BOOL:    return eval_bool(std::move(col));
        case SType::INT8:    return eval_int<int8_t,   2>(std::move(col));
        case SType::INT16:   return eval_int<int16_t,  4>(std::move(col));
        case SType::INT32:   return eval_int<int32_t,  9>(std::move(col));
        case SType::INT64:   return eval_int<int64_t, 19>(std::move(col));
        case SType::FLOAT32: return eval_float<float>(std::move(col));
        case SType::FLOAT64: return eval_float<double>(std::move(col));
        default:
          throw TypeError()
            << "Function datatable.math.round() cannot be applied to a column "
               "of type `" << col.stype() << "`";
      }
    }

  private:
    Column eval_bool(Column&& col) const {
      if (ndigits_ >= 0 || ndigits_ == NODIGITS) {
        return std::move(col);
      }
      return Const_ColumnImpl::make_bool_column(col.nrows(), false);
    }

    template <typename T, int MAXDIGITS>
    Column eval_int(Column&& col) const {
      if (ndigits_ >= 0 || ndigits_ == NODIGITS) {
        return std::move(col);
      }
      if (-ndigits_ <= MAXDIGITS) {
        return Column(new RoundNeg_ColumnImpl<T>(
                            std::move(col),
                            std::pow(10.0, -ndigits_)
                      ));
      }
      return Const_ColumnImpl::make_int_column(col.nrows(), 0, col.stype());
    }

    template <typename T>
    Column eval_float(Column&& col) const {
      if (ndigits_ == NODIGITS) {
        return Column(new Round_ColumnImpl<T, int64_t>(
                          std::move(col),
                          SType::INT64
                      ));
      }
      if (ndigits_ == 0) {
        auto stype = col.stype();
        return Column(new Round_ColumnImpl<T, T>(
                          std::move(col), stype
                      ));
      }
      if (ndigits_ > 0) {
        return Column(new RoundPos_ColumnImpl<double>(
                            std::move(col),
                            std::pow(10.0, ndigits_)
                      ));
      }
      else {
        return Column(new RoundNeg_ColumnImpl<double>(
                            std::move(col),
                            std::pow(10.0, -ndigits_)
                      ));
      }
    }
};




//------------------------------------------------------------------------------
// Python-facing `round()` function
//------------------------------------------------------------------------------

static py::oobj pyfn_round(const py::XArgs& args) {
  auto cols    = args[0].to_oobj();
  auto ndigits = args[1].to<int>(NODIGITS);
  return PyFExpr::make(new FExpr_Round(as_fexpr(cols), ndigits));
}

DECLARE_PYFN(&pyfn_round)
    ->name("round")
    ->docs(doc_math_round)
    ->arg_names({"cols", "ndigits"})
    ->n_positional_args(1)
    ->n_keyword_args(1)
    ->n_required_args(1);




}}  // dt::expr
