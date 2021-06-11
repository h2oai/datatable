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
#include "_dt.h"
#include "expr/fexpr_func_unary.h"
#include "python/xargs.h"
#include "stype.h"
namespace dt {
namespace expr {


//------------------------------------------------------------------------------
// FExpr_AsType
//------------------------------------------------------------------------------

class FExpr_AsType : public FExpr_FuncUnary {
  private:
    dt::Type target_type_;

  public:
    FExpr_AsType(ptrExpr&& arg, dt::Type newtype)
      : FExpr_FuncUnary(std::move(arg)),
        target_type_(newtype)
    {}

    std::string name() const override {
      return "as_type";
    }

    std::string repr() const override {
      std::string out = FExpr_FuncUnary::repr();
      out.erase(out.size() - 1);  // remove ')'
      out += ", ";
      out += target_type_.to_string();
      out += ")";
      return out;
    }

    Column evaluate1(Column&& col) const override {
      col.cast_inplace(target_type_);
      return std::move(col);
    }
};




//------------------------------------------------------------------------------
// Python-facing `as_type()` function
//------------------------------------------------------------------------------

static const char* doc_astype =
R"(as_type(cols, new_type)
--
.. x-version-added:: 1.0

Convert columns `cols` into the prescribed stype.

This function does not modify the data in the original column. Instead
it returns a new column which converts the values into the new type on
the fly.

Parameters
----------
cols: FExpr
    Single or multiple columns that need to be converted.

new_type: Type | stype
    Target type.

return: FExpr
    The output will have the same number of rows and columns as the
    input; column names will be preserved too.


Examples
--------
.. code-block:: python

    >>> from datatable import dt, f, as_type
    >>>
    >>> df = dt.Frame({'A': ['1', '1', '2', '1', '2'],
    ...                'B': [None, '2', '3', '4', '5'],
    ...                'C': [1, 2, 1, 1, 2]})
    >>> df
       | A      B          C
       | str32  str32  int32
    -- + -----  -----  -----
     0 | 1      NA         1
     1 | 1      2          2
     2 | 2      3          1
     3 | 1      4          1
     4 | 2      5          2
    [5 rows x 3 columns]


Convert column A from string to integer type::

    >>> df[:, as_type(f.A, int)]
       |     A
       | int64
    -- + -----
     0 |     1
     1 |     1
     2 |     2
     3 |     1
     4 |     2
    [5 rows x 1 column]


The exact dtype can be specified::

    >>> df[:, as_type(f.A, dt.Type.int32)]
       |     A
       | int32
    -- + -----
     0 |     1
     1 |     1
     2 |     2
     3 |     1
     4 |     2
    [5 rows x 1 column]


Convert multiple columns to different types::

    >>> df[:, [as_type(f.A, int), as_type(f.C, dt.str32)]]
       |     A  C
       | int64  str32
    -- + -----  -----
     0 |     1  1
     1 |     1  2
     2 |     2  1
     3 |     1  1
     4 |     2  2
    [5 rows x 2 columns]
)";

static py::oobj pyfn_astype(const py::XArgs& args) {
  auto cols = args[0].to_oobj();
  dt::Type newtype = args[1].to_type_force();
  return PyFExpr::make(new FExpr_AsType(as_fexpr(cols), newtype));
}

DECLARE_PYFN(&pyfn_astype)
    ->name("as_type")
    ->docs(doc_astype)
    ->arg_names({"cols", "new_type"})
    ->n_positional_args(2)
    ->n_required_args(2);




}}  // dt::expr
