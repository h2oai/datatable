//------------------------------------------------------------------------------
// Copyright 2023 H2O.ai
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
#include <cmath>
#include "column/const.h"
#include "column/func_unary.h"
#include "documentation.h"
#include "expr/fexpr_func_unary.h"
#include "python/xargs.h"
#include "stype.h"
namespace dt {
namespace expr {

template<size_t POS>
class FExpr_Hyperbolic : public FExpr_FuncUnary {
  public:
    using FExpr_FuncUnary::FExpr_FuncUnary;

    static std::string function_name() {
      switch (POS) {
        case 1:
          return "sinh";
        case 2:
          return "cosh";
        case 3:
          return "tanh";
        case 4:
          return "arsinh";
        case 5:
          return "arcosh";
        case 6:
          return "artanh";
      }

    }


    std::string name() const override {
      return function_name();
    }

    /**
      * All standard hyperbolic functions have the same signature:
      *
      *     VOID -> VOID
      *     {BOOL, INT*, FLOAT64} -> FLOAT64
      *     FLOAT32 -> FLOAT32
      *
      */
    template <typename T>
    static inline T op_sinh(T x) {
      return sinh(x);
    }

    template <typename T>
    static inline T op_cosh(T x) {
      return cosh(x);
    }

    template <typename T>
    static inline T op_tanh(T x) {
      return tanh(x);
    }

    template <typename T>
    static inline T op_arsinh(T x) {
      return asinh(x);
    }

    template <typename T>
    static inline T op_arcosh(T x) {
      return acosh(x);
    }

    template <typename T>
    static inline T op_artanh(T x) {
      return atanh(x);
    }

    Column evaluate1(Column&& col) const override{
      SType stype = col.stype();

      switch (stype) {
        case SType::VOID: 
          return Column(new ConstNa_ColumnImpl(col.nrows(), SType::VOID));
        case SType::BOOL: 
        case SType::INT8: 
        case SType::INT16: 
        case SType::INT32:
        case SType::INT64:
          col.cast_inplace(SType::FLOAT64);
          return make<double, POS>(std::move(col));
        case SType::FLOAT32:
          return make<float, POS>(std::move(col));
        case SType::FLOAT64:
          return make<double, POS>(std::move(col));
        default:
          std::string func_name = function_name();
          throw TypeError() << "Function `" << func_name << "` cannot be applied to a "
                               "column of type `" << stype << "`";
      }
    }

  private:
    template <typename T, size_t POSS>
    static Column make(Column&& col) {
      xassert(compatible_type<T>(col.stype()));
      switch (POSS) {
        case 1:
          return Column(new FuncUnary1_ColumnImpl<T, T>(
            std::move(col), op_sinh<T>, 
            col.nrows(), col.stype()
            ));
        case 2:
          return Column(new FuncUnary1_ColumnImpl<T, T>(
            std::move(col), op_cosh<T>, 
            col.nrows(), col.stype()
            ));
        case 3:
          return Column(new FuncUnary1_ColumnImpl<T, T>(
            std::move(col), op_tanh<T>, 
            col.nrows(), col.stype()
            ));
        case 4:
          return Column(new FuncUnary1_ColumnImpl<T, T>(
            std::move(col), op_arsinh<T>, 
            col.nrows(), col.stype()
            ));
        case 5:
          return Column(new FuncUnary1_ColumnImpl<T, T>(
            std::move(col), op_arcosh<T>, 
            col.nrows(), col.stype()
            ));
        case 6:
          return Column(new FuncUnary1_ColumnImpl<T, T>(
            std::move(col), op_artanh<T>, 
            col.nrows(), col.stype()
            ));
      }
      
    }

};

static py::oobj pyfn_sinh(const py::XArgs &args) {
  auto arg = args[0].to_oobj();
  return PyFExpr::make(new FExpr_Hyperbolic<1>(as_fexpr(arg)));
}

static py::oobj pyfn_cosh(const py::XArgs &args) {
  auto arg = args[0].to_oobj();
  return PyFExpr::make(new FExpr_Hyperbolic<2>(as_fexpr(arg)));
}

static py::oobj pyfn_tanh(const py::XArgs &args) {
  auto arg = args[0].to_oobj();
  return PyFExpr::make(new FExpr_Hyperbolic<3>(as_fexpr(arg)));
}

static py::oobj pyfn_arsinh(const py::XArgs &args) {
  auto arg = args[0].to_oobj();
  return PyFExpr::make(new FExpr_Hyperbolic<4>(as_fexpr(arg)));
}

static py::oobj pyfn_arcosh(const py::XArgs &args) {
  auto arg = args[0].to_oobj();
  return PyFExpr::make(new FExpr_Hyperbolic<5>(as_fexpr(arg)));
}

static py::oobj pyfn_artanh(const py::XArgs &args) {
  auto arg = args[0].to_oobj();
  return PyFExpr::make(new FExpr_Hyperbolic<6>(as_fexpr(arg)));
}


DECLARE_PYFN(&pyfn_sinh)
    ->name("sinh")
    ->docs(doc_math_sinh)
    ->arg_names({"cols"})
    ->n_positional_args(1)
    ->n_required_args(1);

DECLARE_PYFN(&pyfn_cosh)
    ->name("cosh")
    ->docs(doc_math_cosh)
    ->arg_names({"cols"})
    ->n_positional_args(1)
    ->n_required_args(1);

DECLARE_PYFN(&pyfn_tanh)
    ->name("tanh")
    ->docs(doc_math_tanh)
    ->arg_names({"cols"})
    ->n_positional_args(1)
    ->n_required_args(1);

DECLARE_PYFN(&pyfn_arsinh)
    ->name("arsinh")
    ->docs(doc_math_arsinh)
    ->arg_names({"cols"})
    ->n_positional_args(1)
    ->n_required_args(1);

DECLARE_PYFN(&pyfn_arcosh)
    ->name("arcosh")
    ->docs(doc_math_arcosh)
    ->arg_names({"cols"})
    ->n_positional_args(1)
    ->n_required_args(1);

DECLARE_PYFN(&pyfn_artanh)
    ->name("artanh")
    ->docs(doc_math_artanh)
    ->arg_names({"cols"})
    ->n_positional_args(1)
    ->n_required_args(1);

}}  // dt::expr