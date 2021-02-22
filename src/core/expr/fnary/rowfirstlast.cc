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
#include <algorithm>
#include "expr/fnary/fnary.h"
#include "column/const.h"
#include "column/func_nary.h"
namespace dt {
namespace expr {



static const char* doc_rowfirst =
R"(rowfirst(cols)
--

For each row, find the first non-missing value in `cols`. If all values
in a row are missing, then this function will also produce a missing value.

Parameters
----------
cols: Expr
    Input columns.

return: Expr
    f-expression consisting of one column and the same number
    of rows as in `cols`.

except: TypeError
    The exception is raised when input columns have incompatible types.

See Also
--------

- :func:`rowlast()` -- find the last non-missing value row-wise.

Example
-------
::

    >>> from datatable import dt, f
    >>> DT = dt.Frame({"A": [1, 1, 2, 1, 2],
    ...                "B": [None, 2, 3, 4, None],
    ...                "C":[True, False, False, True, True]})
    >>> DT
       |     A      B      C
       | int32  int32  bool8
    -- + -----  -----  -----
     0 |     1     NA      1
     1 |     1      2      0
     2 |     2      3      0
     3 |     1      4      1
     4 |     2     NA      1
    [5 rows x 3 columns]

::

    >>> DT[:, dt.rowfirst(f[:])]
       |    C0
       | int32
    -- + -----
     0 |     1
     1 |     1
     2 |     2
     3 |     1
     4 |     2
    [5 rows x 1 column]


::

    >>> DT[:, dt.rowfirst(f['B', 'C'])]
       |    C0
       | int32
    -- + -----
     0 |     1
     1 |     2
     2 |     3
     3 |     4
     4 |     1
    [5 rows x 1 column]




)";


static const char* doc_rowlast =
R"(rowlast(cols)
--

For each row, find the last non-missing value in `cols`. If all values
in a row are missing, then this function will also produce a missing value.

Parameters
----------
cols: Expr
    Input columns.

return: Expr
    f-expression consisting of one column and the same number
    of rows as in `cols`.

except: TypeError
    The exception is raised when input columns have incompatible types.

See Also
--------

- :func:`rowfirst()` -- find the first non-missing value row-wise.

)";


py::PKArgs args_rowfirst(0, 0, 0, true, false, {}, "rowfirst", doc_rowfirst);
py::PKArgs args_rowlast(0, 0, 0, true, false, {}, "rowlast", doc_rowlast);



template <typename T, bool FIRST>
static bool op_rowfirstlast(size_t i, T* out, const colvec& columns) {
  size_t n = columns.size();
  for (size_t j = 0; j < n; ++j) {
    bool xvalid = columns[FIRST? j : n - j - 1].get_element(i, out);
    if (xvalid) return true;
  }
  return false;
}


template <typename T>
static inline Column _rowfirstlast(colvec&& columns, SType outtype, bool FIRST)
{
  auto fn = FIRST? op_rowfirstlast<T, true>
                 : op_rowfirstlast<T, false>;
  size_t nrows = columns[0].nrows();
  return Column(new FuncNary_ColumnImpl<T>(
                    std::move(columns), fn, nrows, outtype));
}



Column naryop_rowfirstlast(colvec&& columns, bool FIRST) {
  if (columns.empty()) {
    return Const_ColumnImpl::make_na_column(1);
  }
  const char* fnname = FIRST? "rowfirst" : "rowlast";

  // Detect common stype
  SType stype0 = SType::VOID;
  for (const auto& col : columns) {
    stype0 = common_stype(stype0, col.stype());
  }
  if (stype0 == SType::INVALID) {
    throw TypeError() << "Incompatible column types in function `" << fnname << "`";
  }
  promote_columns(columns, stype0);

  switch (stype0) {
    case SType::BOOL:    return _rowfirstlast<int8_t>(std::move(columns), stype0, FIRST);
    case SType::INT8:    return _rowfirstlast<int8_t>(std::move(columns), stype0, FIRST);
    case SType::INT16:   return _rowfirstlast<int16_t>(std::move(columns), stype0, FIRST);
    case SType::INT32:   return _rowfirstlast<int32_t>(std::move(columns), stype0, FIRST);
    case SType::INT64:   return _rowfirstlast<int64_t>(std::move(columns), stype0, FIRST);
    case SType::FLOAT32: return _rowfirstlast<float>(std::move(columns), stype0, FIRST);
    case SType::FLOAT64: return _rowfirstlast<double>(std::move(columns), stype0, FIRST);
    case SType::STR32:
    case SType::STR64:   return _rowfirstlast<CString>(std::move(columns), stype0, FIRST);
    default: {
      throw TypeError() << "Unknown type " << stype0;
    }
  }
}




}}  // namespace dt::expr
