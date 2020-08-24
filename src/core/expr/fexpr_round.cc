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
#include <type_traits>
#include <limits>
#include "_dt.h"
#include "column/const.h"
#include "column/virtual.h"
#include "expr/eval_context.h"
#include "expr/fexpr_func_unary.h"
#include "python/xargs.h"
namespace dt {
namespace expr {

static constexpr int NODIGITS = std::numeric_limits<int>::min();


//------------------------------------------------------------------------------
// RoundNeg_ColumnImpl
//------------------------------------------------------------------------------

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
      xassert(compatible_type<T>(arg_.stype()));
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

template <typename T>
class RoundPos_ColumnImpl : public Virtual_ColumnImpl {
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
      auto nrows = col.nrows();
      auto stype = col.stype();

      switch (stype) {
        case SType::VOID: return std::move(col);
        case SType::BOOL:
        case SType::INT8:
        case SType::INT16:
        case SType::INT32:
        case SType::INT64: {
          if (ndigits_ >= 0 || ndigits_ == NODIGITS) {
            return std::move(col);
          }
          int maxdigits = (stype == SType::INT64)? 19 :
                          (stype == SType::INT32)? 9 :
                          (stype == SType::INT16)? 4 :
                          (stype == SType::INT8)?  2 : 0;
          if (-ndigits_ <= maxdigits) {
            return Column(new RoundNeg_ColumnImpl<int8_t>(
                                std::move(col),
                                std::pow(10.0, -ndigits_)
                          ));
          }
          if (stype == SType::BOOL) {
            return Const_ColumnImpl::make_bool_column(nrows, false);
          } else {
            return Const_ColumnImpl::make_int_column(nrows, 0, stype);
          }
        }
        case SType::FLOAT64: {
          if (ndigits_ >= 19) return std::move(col);
          if (ndigits_ >= 0) {
            return Column(new RoundPos_ColumnImpl<double>(
                                std::move(col),
                                std::pow(10.0, ndigits_)
                          ));
          }
          if (ndigits_ >= -19) {
            return Column(new RoundNeg_ColumnImpl<double>(
                                std::move(col),
                                std::pow(10.0, -ndigits_)
                          ));
          }
          return Const_ColumnImpl::make_float_column(nrows, 0);  // ???
        }
        default:
          throw TypeError()
            << "Function datatable.math.round() cannot be applied to a column "
               "of type `" << col.stype() << "`";
      }
    }
};




//------------------------------------------------------------------------------
// Python-facing `round()` function
//------------------------------------------------------------------------------

static const char* doc_round =
R"(round(cols, ndigits=None)
--

Round the values in `cols` up to the specified number of the digits
of precision `ndigits`. If the number of digits is omitted, rounds
to the nearest integer.

Parameters
----------
cols: FExpr
    Input data for rounding.

ndigits: int | None

return: FExpr
    f-expression that rounds the values in its first argument to
    the specified number of precision digits.
)";

static py::oobj pyfn_round(const py::XArgs& args) {
  auto cols    = args[0].to_oobj();
  auto ndigits = args[1].to<int>(NODIGITS);
  return PyFExpr::make(new FExpr_Round(as_fexpr(cols), ndigits));
}

DECLARE_PYFN(&pyfn_round)
    ->name("round")
    ->docs(doc_round)
    ->arg_names({"cols", "ndigits"})
    ->n_positional_args(1)
    ->n_keyword_args(1)
    ->n_required_args(1);




}}  // dt::expr
