//------------------------------------------------------------------------------
// Copyright 2019-2021 H2O.ai
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
#include "column/const.h"
#include "column/func_nary.h"
#include "expr/fnary/fnary.h"
#include "models/utils.h"
#include "python/xargs.h"
namespace dt {
namespace expr {



std::string FExpr_RowSum::name() const {
  return "rowsum";
}


template <typename T>
static bool op_rowsum(size_t i, T* out, const colvec& columns) {
  T sum = 0;
  for (const auto& col : columns) {
    T x;
    bool xvalid = col.get_element(i, &x);
    if (xvalid) {
      sum += x;
    }
  }
  *out = sum;
  return _notnan(sum);
}


template <typename T>
static inline Column _rowsum(colvec&& columns) {
  size_t nrows = columns[0].nrows();
  return Column(new FuncNary_ColumnImpl<T>(
                    std::move(columns), op_rowsum<T>, nrows, stype_from<T>));
}


Column FExpr_RowSum::apply_function(colvec&& columns) const {
  if (columns.empty()) {
    return Const_ColumnImpl::make_int_column(1, 0, SType::INT32);
  }
  SType res_stype = common_numeric_stype(columns);
  promote_columns(columns, res_stype);

  switch (res_stype) {
    case SType::INT32:   return _rowsum<int32_t>(std::move(columns));
    case SType::INT64:   return _rowsum<int64_t>(std::move(columns));
    case SType::FLOAT32: return _rowsum<float>(std::move(columns));
    case SType::FLOAT64: return _rowsum<double>(std::move(columns));
    default: throw RuntimeError()
               << "Wrong `res_stype` in `naryop_rowsum()`: "
               << res_stype;  // LCOV_EXCL_LINE
  }
}



static const char* doc_rowsum =
R"(rowsum(*cols)
--

For each row, calculate the sum of all values in `cols`. Missing values
are treated as if they are zeros and skipped during the calcultion.


Parameters
----------
cols: FExpr
    Input columns.

return: FExpr
    f-expression consisting of one column and the same number
    of rows as in `cols`. The stype of the resulting column
    will be the smallest common stype calculated for `cols`,
    but not less than `int32`.

except: TypeError
    The exception is raised when one of the columns from `cols`
    has a non-numeric type.

Examples
--------
::

    >>> from datatable import dt, f, rowsum
    >>> DT = dt.Frame({'a': [1,2,3],
    ...                'b': [2,3,4],
    ...                'c':['dd','ee','ff'],
    ...                'd':[5,9,1]})
    >>> DT
       |     a      b  c          d
       | int32  int32  str32  int32
    -- + -----  -----  -----  -----
     0 |     1      2  dd         5
     1 |     2      3  ee         9
     2 |     3      4  ff         1
    [3 rows x 4 columns]


::

    >>> DT[:, rowsum(f[int])]
       |    C0
       | int32
    -- + -----
     0 |     8
     1 |    14
     2 |     8
    [3 rows x 1 column]

::

    >>> DT[:, rowsum(f.a, f.b)]
       |    C0
       | int32
    -- + -----
     0 |     3
     1 |     5
     2 |     7
    [3 rows x 1 column]

The above code could also be written as ::

    >>> DT[:, f.a + f.b]
       |    C0
       | int32
    -- + -----
     0 |     3
     1 |     5
     2 |     7
    [3 rows x 1 column]



See Also
--------
- :func:`rowcount()` -- count non-missing values row-wise.
)";

DECLARE_PYFN(&py_rowfn)
    ->name("rowsum")
    ->docs(doc_rowsum)
    ->allow_varargs()
    ->add_info(FN_ROWSUM);




}}  // namespace dt::expr
