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
class FExpr_Trigonometric : public FExpr_FuncUnary {
  public:
    using FExpr_FuncUnary::FExpr_FuncUnary;

    static std::string function_name() {
      switch (POS) {
        case 1:
          return "sin";
        case 2:
          return "cos";
        case 3:
          return "tan";
        case 4:
          return "arcsin";
        case 5:
          return "arccos";
        case 6:
          return "arctan";
        case 7:
          return "deg2rad";
        case 8:
          return "rag2deg";
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
    template <typename T, size_t POSN>
    static inline T op_trigonometric(T x) {
      switch (POSN) {
        case 1:
          return sin(x);
        case 2:
          return cos(x);
        case 3:
          return tan(x);
        case 4:
          return asin(x);
        case 5:
          return acos(x);
        case 6:
          return atan(x);
      }
    }

    template <typename T>
    static inline T f64_deg2rad(T x) {
      static constexpr T RADIANS_IN_DEGREE = 0.017453292519943295;
      return x * RADIANS_IN_DEGREE;
    }

    template <typename T>
    static T f32_deg2rad(T x) {
      static constexpr T RADIANS_IN_DEGREE = 0.0174532924f;
      return x * RADIANS_IN_DEGREE;
    }

    template <typename T>
    static T f64_rad2deg(T x) {
      static constexpr T DEGREES_IN_RADIAN = 57.295779513082323;
      return x * DEGREES_IN_RADIAN;
    }

    template <typename T>
    static T f32_rad2deg(T x) {
      static constexpr T DEGREES_IN_RADIAN = 57.2957802f;
      return x * DEGREES_IN_RADIAN;
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
      if (POSS < 7){
        return Column(new FuncUnary1_ColumnImpl<T, T>(
          std::move(col), op_trigonometric<T, POSS>, 
          col.nrows(), col.stype()
          ));    
      }
      if (POSS == 7) {
        if (col.stype() == SType::FLOAT64) {
          return Column(new FuncUnary1_ColumnImpl<double, double>(
            std::move(col), f64_deg2rad<double>, 
            col.nrows(), col.stype()
            )); 
        }
        return Column(new FuncUnary1_ColumnImpl<float, float>(
          std::move(col), f32_deg2rad<float>, 
          col.nrows(), col.stype()
          ));    

      }
      if (POSS == 8) {
        if (col.stype() == SType::FLOAT64) {
          return Column(new FuncUnary1_ColumnImpl<double, double>(
            std::move(col), f64_rad2deg<double>, 
            col.nrows(), col.stype()
            )); 
        }
        return Column(new FuncUnary1_ColumnImpl<float, float>(
          std::move(col), f32_rad2deg<float>, 
          col.nrows(), col.stype()
          ));    

      }     
    }

};

static py::oobj pyfn_sin(const py::XArgs &args) {
  auto arg = args[0].to_oobj();
  return PyFExpr::make(new FExpr_Trigonometric<1>(as_fexpr(arg)));
}

static py::oobj pyfn_cos(const py::XArgs &args) {
  auto arg = args[0].to_oobj();
  return PyFExpr::make(new FExpr_Trigonometric<2>(as_fexpr(arg)));
}

static py::oobj pyfn_tan(const py::XArgs &args) {
  auto arg = args[0].to_oobj();
  return PyFExpr::make(new FExpr_Trigonometric<3>(as_fexpr(arg)));
}

static py::oobj pyfn_arcsin(const py::XArgs &args) {
  auto arg = args[0].to_oobj();
  return PyFExpr::make(new FExpr_Trigonometric<4>(as_fexpr(arg)));
}

static py::oobj pyfn_arccos(const py::XArgs &args) {
  auto arg = args[0].to_oobj();
  return PyFExpr::make(new FExpr_Trigonometric<5>(as_fexpr(arg)));
}

static py::oobj pyfn_arctan(const py::XArgs &args) {
  auto arg = args[0].to_oobj();
  return PyFExpr::make(new FExpr_Trigonometric<6>(as_fexpr(arg)));
}

static py::oobj pyfn_deg2rad(const py::XArgs &args) {
  auto arg = args[0].to_oobj();
  return PyFExpr::make(new FExpr_Trigonometric<7>(as_fexpr(arg)));
}

static py::oobj pyfn_rad2deg(const py::XArgs &args) {
  auto arg = args[0].to_oobj();
  return PyFExpr::make(new FExpr_Trigonometric<8>(as_fexpr(arg)));
}

DECLARE_PYFN(&pyfn_sin)
    ->name("sin")
    ->docs(doc_math_sin)
    ->arg_names({"cols"})
    ->n_positional_args(1)
    ->n_required_args(1);

DECLARE_PYFN(&pyfn_cos)
    ->name("cos")
    ->docs(doc_math_cos)
    ->arg_names({"cols"})
    ->n_positional_args(1)
    ->n_required_args(1);

DECLARE_PYFN(&pyfn_tan)
    ->name("tan")
    ->docs(doc_math_tan)
    ->arg_names({"cols"})
    ->n_positional_args(1)
    ->n_required_args(1);

DECLARE_PYFN(&pyfn_arcsin)
    ->name("arcsin")
    ->docs(doc_math_arcsin)
    ->arg_names({"cols"})
    ->n_positional_args(1)
    ->n_required_args(1);

DECLARE_PYFN(&pyfn_arccos)
    ->name("arccos")
    ->docs(doc_math_arccos)
    ->arg_names({"cols"})
    ->n_positional_args(1)
    ->n_required_args(1);

DECLARE_PYFN(&pyfn_arctan)
    ->name("arctan")
    ->docs(doc_math_arctan)
    ->arg_names({"cols"})
    ->n_positional_args(1)
    ->n_required_args(1);

DECLARE_PYFN(&pyfn_deg2rad)
    ->name("deg2rad")
    ->docs(doc_math_deg2rad)
    ->arg_names({"cols"})
    ->n_positional_args(1)
    ->n_required_args(1);

DECLARE_PYFN(&pyfn_rad2deg)
    ->name("rad2deg")
    ->docs(doc_math_rad2deg)
    ->arg_names({"cols"})
    ->n_positional_args(1)
    ->n_required_args(1);

}}  // dt::expr

