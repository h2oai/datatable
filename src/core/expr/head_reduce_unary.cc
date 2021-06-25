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
#include <unordered_map>
#include "column/latent.h"
#include "column/virtual.h"
#include "expr/eval_context.h"
#include "expr/expr.h"
#include "expr/head_reduce.h"
#include "expr/workframe.h"
#include "utils/assert.h"
#include "utils/exceptions.h"
#include "stype.h"
#include <iostream>
namespace dt {
namespace expr {


static Error _error(const char* name, SType stype) {
  return TypeError() << "Unable to apply reduce function `"
          << name << "()` to a column of type `" << stype << "`";
}



template <typename U>
using reducer_fn = bool(*)(const Column&, size_t, size_t, U*);

using maker_fn = Column(*)(Column&&, const Groupby&);



//------------------------------------------------------------------------------
// Reduced_ ColumnImpl
//------------------------------------------------------------------------------

// T - type of elements in the `arg` column
// U - type of output elements from this column
//
template <typename T, typename U>
class Reduced_ColumnImpl : public Virtual_ColumnImpl {
  private:
    Column arg;
    Groupby groupby;
    reducer_fn<U> reducer;

    // Each element is "expensive" to compute if the average group
    // size is larger than this threshold.
    static constexpr size_t GROUP_SIZE_TINY = 4;

  public:
    Reduced_ColumnImpl(SType stype, Column&& col, const Groupby& grpby,
                       reducer_fn<U> fn)
      : Virtual_ColumnImpl(grpby.size(), stype),
        arg(std::move(col)),
        groupby(grpby),
        reducer(fn)
    {
      xassert(compatible_type<T>(arg.stype()));
      xassert(compatible_type<U>(stype));
    }

    ColumnImpl* clone() const override {
      return new Reduced_ColumnImpl<T, U>(stype(), Column(arg), groupby,
                                          reducer);
    }

    size_t n_children() const noexcept override {
      return 1;
    }

    const Column& child(size_t i) const override {
      xassert(i == 0);  (void)i;
      return arg;
    }


    bool get_element(size_t i, U* out) const override {
      size_t i0, i1;
      groupby.get_group(i, &i0, &i1);
      return reducer(arg, i0, i1, out);
    }

    bool computationally_expensive() const override {
      return nrows_ >= GROUP_SIZE_TINY * groupby.size();
    }
};




//------------------------------------------------------------------------------
// first(A), last(A)
//------------------------------------------------------------------------------
#if 0
static const char* doc_first =
R"(first(cols)
--

Return the first row for each column from `cols`.

Parameters
----------
cols: Expr
    Input columns.

return: Expr
    f-expression having one row, and the same names, stypes and
    number of columns as in `cols`.


Examples
--------
:func:`first()` returns the first column in a frame::

    >>> from datatable import dt, f, by, sort, first
    >>> df = dt.Frame({"A": [1, 1, 2, 1, 2],
    ...                "B": [None, 2, 3, 4, 5]})
    >>> df
       |  A   B
    -- + --  --
     0 |  1  NA
     1 |  1   2
     2 |  2   3
     3 |  1   4
     4 |  2   5

    [5 rows x 2 columns]
    >>> dt.first(df)
       |  A
    -- + --
     0 |  1
     1 |  1
     2 |  2
     3 |  1
     4 |  2

    [5 rows x 1 column]

Within a frame, it returns the first row::

    >>> df[:, first(f[:])]
       |  A   B
    -- + --  --
     0 |  1  NA

    [1 row x 2 columns]

Of course, you can replicate this by passing 0 to the ``i`` section instead::

    >>> df[0, :]
       |  A   B
    -- + --  --
     0 |  1  NA

    [1 row x 2 columns]

:func:`first()` comes in handy if you wish to get the first non null value in a
column::

    >>> df[f.B != None, first(f.B)]
       |  B
    -- + --
     0 |  2

    [1 row x 1 column]

:func:`first()` returns the first row per group in a :func:`by()` operation::

    >>> df[:, first(f[:]), by("A")]
       |  A   B
    -- + --  --
     0 |  1  NA
     1 |  2   3

    [2 rows x 2 columns]

To get the first non-null value per row in a :func:`by()` operation, you can
use the :func:`sort()` function, and set the ``na_position`` argument as
``last``::

    >>> df[:, first(f[:]), by("A"), sort("B", na_position="last")]
       |  A   B
    -- + --  --
     0 |  1   2
     1 |  2   3

    [2 rows x 2 columns]


See Also
--------
- :func:`last()` -- function that returns the last row.
)";


static const char* doc_last =
R"(last(cols)
--

Return the last row for each column from `cols`.

Parameters
----------
cols: Expr
    Input columns.

return: Expr
    f-expression having one row, and the same names, stypes and
    number of columns as in `cols`.


Examples
--------

``last()`` returns the last column in a frame::

    >>> from datatable import dt, f, by, sort, last
    >>>
    >>> df = dt.Frame({"A": [1, 1, 2, 1, 2],
    ...                "B": [None, 2, 3, 4, None]})
    >>>
    >>> df
       |     A      B
       | int32  int32
    -- + -----  -----
     0 |     1     NA
     1 |     1      2
     2 |     2      3
     3 |     1      4
     4 |     2     NA
    [5 rows x 2 columns]

    >>> dt.last(df)
       |     B
       | int32
    -- + -----
     0 |    NA
     1 |     2
     2 |     3
     3 |     4
     4 |    NA
    [5 rows x 1 column]

Within a frame, it returns the last row::

    >>> df[:, last(f[:])]
       |     A      B
       | int32  int32
    -- + -----  -----
     0 |     2     NA
    [1 row x 2 columns]

The above code can be replicated by passing -1 to the ``i`` section instead::

    >>> df[-1, :]
       |     A      B
       | int32  int32
    -- + -----  -----
     0 |     2     NA
    [1 row x 2 columns]

Like ``first()``, ``last()`` can be handy if you wish to get the last
non null value in a column::

    >>> df[f.B != None, dt.last(f.B)]
       |     B
       | int32
    -- + -----
     0 |     4
    [1 row x 1 column]

``last()`` returns the last row per group in a :func:`by()` operation::

    >>> df[:, last(f[:]), by("A")]
       |     A      B
       | int32  int32
    -- + -----  -----
     0 |     1      4
     1 |     2     NA
    [2 rows x 2 columns]

To get the last non-null value per row in a :func:`by()` operation, you can
use the :func:`sort()` function, and set the ``na_position`` argument as
``first`` (this will move the ``NAs`` to the top of the column)::

    >>> df[:, last(f[:]), by("A"), sort("B", na_position="first")]
       |     A      B
       | int32  int32
    -- + -----  -----
     0 |     1      4
     1 |     2      3
    [2 rows x 2 columns]


See Also
--------
- :func:`first()` -- function that returns the first row.
)";
#endif

template <bool FIRST>
class FirstLast_ColumnImpl : public Virtual_ColumnImpl {
  private:
    Column arg;
    Groupby groupby;

  public:
    FirstLast_ColumnImpl(Column&& col, const Groupby& grpby)
      : Virtual_ColumnImpl(grpby.size(), col.stype()),
        arg(std::move(col)),
        groupby(grpby) {}

    ColumnImpl* clone() const override {
      return new FirstLast_ColumnImpl<FIRST>(Column(arg), groupby);
    }

    bool get_element(size_t i, int8_t*   out) const override { return _get(i, out); }
    bool get_element(size_t i, int16_t*  out) const override { return _get(i, out); }
    bool get_element(size_t i, int32_t*  out) const override { return _get(i, out); }
    bool get_element(size_t i, int64_t*  out) const override { return _get(i, out); }
    bool get_element(size_t i, float*    out) const override { return _get(i, out); }
    bool get_element(size_t i, double*   out) const override { return _get(i, out); }
    bool get_element(size_t i, CString*  out) const override { return _get(i, out); }
    bool get_element(size_t i, py::oobj* out) const override { return _get(i, out); }


    bool computationally_expensive() const override {
      return true;
    }

    size_t n_children() const noexcept override {
      return 1;
    }

    const Column& child(size_t i) const override {
      xassert(i == 0);  (void)i;
      return arg;
    }


  private:
    template <typename T>
    bool _get(size_t i, T* out) const {
      size_t i0, i1;
      groupby.get_group(i, &i0, &i1);
      xassert(i0 < i1);
      return FIRST? arg.get_element(i0, out)
                  : arg.get_element(i1 - 1, out);
    }
};


template <bool FIRST>
static Column compute_firstlast(Column&& arg, const Groupby& gby) {
  if (arg.nrows() == 0) {
    return Column::new_na_column(1, arg.stype());
  }
  else {
    return Column(new FirstLast_ColumnImpl<FIRST>(std::move(arg), gby));
  }
}


static Column compute_gfirstlast(Column&& arg, const Groupby&) {
  return (arg.nrows() == 0)? Column::new_na_column(1, arg.stype())
                           : std::move(arg);
}




//------------------------------------------------------------------------------
// sum(A)
//------------------------------------------------------------------------------
#if 0
static const char* doc_sum =
R"(sum(cols)
--

Calculate the sum of values for each column from `cols`.

Parameters
----------
cols: Expr
    Input columns.

return: Expr
    f-expression having one row, and the same names and number of columns
    as in `cols`. The column stypes are `int64` for
    boolean and integer columns, `float32` for `float32` columns
    and `float64` for `float64` columns.

except: TypeError
    The exception is raised when one of the columns from `cols`
    has a non-numeric type.


Examples
--------

.. code-block:: python

    >>> from datatable import dt, f, by
    >>>
    >>> df = dt.Frame({'A': [1, 1, 2, 1, 2],
    ...                'B': [None, 2, 3,4, 5],
    ...                'C': [1, 2, 1, 1, 2]})
    >>> df
       |     A      B      C
       | int32  int32  int32
    -- + -----  -----  -----
     0 |     1     NA      1
     1 |     1      2      2
     2 |     2      3      1
     3 |     1      4      1
     4 |     2      5      2
    [5 rows x 3 columns]

Get the sum of column A::

    >>> df[:, dt.sum(f.A)]
       |     A
       | int64
    -- + -----
     0 |     7
    [1 row x 1 column]

Get the sum of multiple columns::

    >>> df[:, [dt.sum(f.A), dt.sum(f.B)]]
       |     A      B
       | int64  int64
    -- + -----  -----
     0 |     7     14
    [1 row x 2 columns]

Same as above, but more convenient::

    >>> df[:, dt.sum(f[:2])]
       |     A      B
       | int64  int64
    -- + -----  -----
     0 |     7     14
    [1 row x 2 columns]

In the presence of :func:`by()`, it returns the sum of the specified columns per group::

    >>> df[:, [dt.sum(f.A), dt.sum(f.B)], by(f.C)]
       |     C      A      B
       | int32  int64  int64
    -- + -----  -----  -----
     0 |     1      4      7
     1 |     2      3      7
    [2 rows x 3 columns]


See Also
--------
- :func:`count()` -- function to calculate a number of non-missing values.
)";
#endif

template <typename T, typename U>
bool sum_reducer(const Column& col, size_t i0, size_t i1, U* out) {
  U sum = 0;
  for (size_t i = i0; i < i1; ++i) {
    T value;
    bool isvalid = col.get_element(i, &value);
    if (isvalid) {
      sum += static_cast<U>(value);
    }
  }
  *out = sum;
  return true;  // *out is not NA
}



template <typename T, typename U>
static Column _sum(Column&& arg, const Groupby& gby) {
  return Column(
          new Latent_ColumnImpl(
            new Reduced_ColumnImpl<T, U>(
                 stype_from<U>, std::move(arg), gby, sum_reducer<T, U>
            )));
}

static Column compute_sum(Column&& arg, const Groupby& gby) {
  switch (arg.stype()) {
    case SType::BOOL:
    case SType::INT8:    return _sum<int8_t, int64_t>(std::move(arg), gby);
    case SType::INT16:   return _sum<int16_t, int64_t>(std::move(arg), gby);
    case SType::INT32:   return _sum<int32_t, int64_t>(std::move(arg), gby);
    case SType::INT64:   return _sum<int64_t, int64_t>(std::move(arg), gby);
    case SType::FLOAT32: return _sum<float, float>(std::move(arg), gby);
    case SType::FLOAT64: return _sum<double, double>(std::move(arg), gby);
    default: throw _error("sum", arg.stype());
  }
}




//------------------------------------------------------------------------------
// sum(A:grouped)
//------------------------------------------------------------------------------

template <typename T, typename U>
bool sum_greducer(const Column& col, size_t i0, size_t i1, U* out) {
  T value;
  bool isvalid = col.get_element(i0, &value);
  *out = isvalid? static_cast<U>(i1 - i0) * static_cast<U>(value)
                : U(0);
  return true;  // *out is not NA
}



template <typename T, typename U>
static Column _gsum(Column&& arg, const Groupby& gby) {
  return Column(
            new Reduced_ColumnImpl<T, U>(
                 stype_from<U>, std::move(arg), gby, sum_greducer<T, U>
            ));
}

static Column compute_gsum(Column&& arg, const Groupby& gby) {
  switch (arg.stype()) {
    case SType::BOOL:
    case SType::INT8:    return _gsum<int8_t,  int64_t>(std::move(arg), gby);
    case SType::INT16:   return _gsum<int16_t, int64_t>(std::move(arg), gby);
    case SType::INT32:   return _gsum<int32_t, int64_t>(std::move(arg), gby);
    case SType::INT64:   return _gsum<int64_t, int64_t>(std::move(arg), gby);
    case SType::FLOAT32: return _gsum<float,   float>  (std::move(arg), gby);
    case SType::FLOAT64: return _gsum<double,  double> (std::move(arg), gby);
    default: throw _error("sum", arg.stype());
  }
}




//------------------------------------------------------------------------------
// mean(A)
//------------------------------------------------------------------------------

#if 0
static const char* doc_mean =
R"(mean(cols)
--

Calculate the mean value for each column from `cols`.

Parameters
----------
cols: Expr
    Input columns.

return: Expr
    f-expression having one row, and the same names and number of columns
    as in `cols`. The column stypes are `float32` for
    `float32` columns, and `float64` for all the other numeric types.

except: TypeError
    The exception is raised when one of the columns from `cols`
    has a non-numeric type.

See Also
--------

- :func:`median()` -- function to calculate median values.
- :func:`sd()` -- function to calculate standard deviation.

Examples
--------
.. code-block:: python

    >>> from datatable import dt, f, by
    >>>
    >>> df = dt.Frame({'A': [1, 1, 2, 1, 2],
    ...                'B': [None, 2, 3,4, 5],
    ...                'C': [1, 2, 1, 1, 2]})
    >>>
    >>> df
       |     A      B      C
       | int32  int32  int32
    -- + -----  -----  -----
     0 |     1     NA      1
     1 |     1      2      2
     2 |     2      3      1
     3 |     1      4      1
     4 |     2      5      2
    [5 rows x 3 columns]


Get the mean from column A::

    >>> df[:, dt.mean(f.A)]
       |       A
       | float64
    -- + -------
     0 |     1.4
    [1 row x 1 column]

Get the mean of multiple columns::

    >>> df[:, dt.mean([f.A, f.B])]
       |       A        B
       | float64  float64
    -- + -------  -------
     0 |     1.4      3.5
    [1 row x 2 columns]

Same as above, but applying to a column slice::

    >>> df[:, dt.mean(f[:2])]
       |       A        B
       | float64  float64
    -- + -------  -------
     0 |     1.4      3.5
    [1 row x 2 columns]

You can pass in a dictionary with new column names::

    >>> df[:, dt.mean({"A_mean": f.A, "C_avg": f.C})]
       |  A_mean    C_avg
       | float64  float64
    -- + -------  -------
     0 |     1.4      1.4
    [1 row x 2 columns]

In the presence of :func:`by()`, it returns the average of each column per group::

    >>> df[:, dt.mean({"A_mean": f.A, "B_mean": f.B}), by("C")]
       |     C   A_mean   B_mean
       | int32  float64  float64
    -- + -----  -------  -------
     0 |     1  1.33333      3.5
     1 |     2  1.5          3.5
    [2 rows x 3 columns]
)";
#endif


template <typename T, typename U>
bool mean_reducer(const Column& col, size_t i0, size_t i1, U* out) {
  double sum = 0;
  int64_t count = 0;
  for (size_t i = i0; i < i1; ++i) {
    T value;
    bool isvalid = col.get_element(i, &value);
    if (isvalid) {
      sum += static_cast<double>(value);
      count++;
    }
  }
  if (!count) return false;
  *out = static_cast<U>(sum / static_cast<double>(count));
  return true;  // *out is not NA
}



template <typename T>
static Column _mean(Column&& arg, const Groupby& gby) {
  using U = typename std::conditional<std::is_same<T, float>::value,
                                      float, double>::type;
  return Column(
          new Latent_ColumnImpl(
            new Reduced_ColumnImpl<T, U>(
                 stype_from<U>, std::move(arg), gby, mean_reducer<T, U>
            )));
}

static Column compute_mean(Column&& arg, const Groupby& gby) {
  switch (arg.stype()) {
    case SType::BOOL:
    case SType::INT8:    return _mean<int8_t> (std::move(arg), gby);
    case SType::INT16:   return _mean<int16_t>(std::move(arg), gby);
    case SType::INT32:   return _mean<int32_t>(std::move(arg), gby);
    case SType::INT64:   return _mean<int64_t>(std::move(arg), gby);
    case SType::FLOAT32: return _mean<float>  (std::move(arg), gby);
    case SType::FLOAT64: return _mean<double> (std::move(arg), gby);
    case SType::TIME64: {
      auto col = _mean<int64_t>(std::move(arg), gby);
      col.cast_inplace(SType::TIME64);
      return col;
    }
    default: throw _error("mean", arg.stype());
  }
}

static Column compute_gmean(Column&& arg, const Groupby&) {
  SType arg_stype = arg.stype();
  if (arg_stype == SType::STR32 || arg_stype == SType::STR64) {
    throw _error("mean", arg_stype);
  }
  SType res_stype = (arg_stype == SType::FLOAT32)? SType::FLOAT32
                                                 : SType::FLOAT64;
  if (arg.nrows() == 0) {
    return Column::new_na_column(1, res_stype);
  }
  arg.cast_inplace(res_stype);
  return std::move(arg);
}




//------------------------------------------------------------------------------
// sd(A)
//------------------------------------------------------------------------------

#if 0
static const char* doc_sd =
R"(sd(cols)
--

Calculate the standard deviation for each column from `cols`.

Parameters
----------
cols: Expr
    Input columns.

return: Expr
    f-expression having one row, and the same names and number of columns
    as in `cols`. The column stypes are `float32` for
    `float32` columns, and `float64` for all the other numeric types.

except: TypeError
    The exception is raised when one of the columns from `cols`
    has a non-numeric type.


Examples
--------

.. code-block:: python

    >>> from datatable import dt, f
    >>>
    >>> DT = dt.Frame(A = [0, 1, 2, 3], B = [0, 2, 4, 6])
    >>> DT
       |     A      B
       | int32  int32
    -- + -----  -----
     0 |     0      0
     1 |     1      2
     2 |     2      4
     3 |     3      6
    [4 rows x 2 columns]


Get the standard deviation of column A::

    >>> DT[:, dt.sd(f.A)]
       |       A
       | float64
    -- + -------
     0 | 1.29099
    [1 row x 1 column]


Get the standard deviation of columns A and B::

    >>> DT[:, dt.sd([f.A, f.B])]
       |       A        B
       | float64  float64
    -- + -------  -------
     0 | 1.29099  2.58199
    [1 row x 2 columns]


See Also
--------

- :func:`mean()` -- function to calculate mean values.
- :func:`median()` -- function to calculate median values.

)";
#endif

template <typename T, typename U>
bool sd_reducer(const Column& col, size_t i0, size_t i1, U* out) {
  double mean = 0;
  double m2 = 0;
  T value;
  int64_t count = 0;
  for (size_t i = i0; i < i1; ++i) {
    bool isvalid = col.get_element(i, &value);
    if (isvalid) {
      count++;
      double tmp1 = static_cast<double>(value) - mean;
      mean += tmp1 / static_cast<double>(count);
      double tmp2 = static_cast<double>(value) - mean;
      m2 += tmp1 * tmp2;
    }
  }
  if (count <= 1) return false;
  // In theory, m2 should always be positive, but perhaps it could
  // occasionally become negative due to round-off errors?
  *out = static_cast<U>(m2 >= 0? std::sqrt(m2/static_cast<double>(count - 1))
                               : 0.0);
  return true;  // *out is not NA
}



template <typename T>
static Column _sd(Column&& arg, const Groupby& gby) {
  using U = typename std::conditional<std::is_same<T, float>::value,
                                      float, double>::type;
  return Column(
          new Latent_ColumnImpl(
            new Reduced_ColumnImpl<T, U>(
                 stype_from<U>, std::move(arg), gby, sd_reducer<T, U>
            )));
}

static Column compute_sd(Column&& arg, const Groupby& gby) {
  switch (arg.stype()) {
    case SType::BOOL:
    case SType::INT8:    return _sd<int8_t> (std::move(arg), gby);
    case SType::INT16:   return _sd<int16_t>(std::move(arg), gby);
    case SType::INT32:   return _sd<int32_t>(std::move(arg), gby);
    case SType::INT64:   return _sd<int64_t>(std::move(arg), gby);
    case SType::FLOAT32: return _sd<float>  (std::move(arg), gby);
    case SType::FLOAT64: return _sd<double> (std::move(arg), gby);
    default: throw _error("sd", arg.stype());
  }
}




//------------------------------------------------------------------------------
// sd(A:grouped)
//------------------------------------------------------------------------------

class SdGrouped_ColumnImpl : public Virtual_ColumnImpl {
  private:
    Column arg;
    Groupby groupby;

  public:
    SdGrouped_ColumnImpl(SType stype, Column&& col, const Groupby& grpby)
      : Virtual_ColumnImpl(grpby.size(), stype),
        arg(std::move(col)),
        groupby(grpby) {}

    ColumnImpl* clone() const override {
      return new SdGrouped_ColumnImpl(stype(), Column(arg), groupby);
    }

    bool get_element(size_t i, float* out) const override {
      *out = 0.0f;
      size_t i0, i1;
      groupby.get_group(i, &i0, &i1);
      return (i1 - i0 > 1);
    }

    bool get_element(size_t i, double* out) const override {
      *out = 0.0;
      size_t i0, i1;
      groupby.get_group(i, &i0, &i1);
      return (i1 - i0 > 1);
    }


    size_t n_children() const noexcept override {
      return 1;
    }

    const Column& child(size_t i) const override {
      xassert(i == 0);  (void)i;
      return arg;
    }
};


static Column compute_gsd(Column&& arg, const Groupby& gby) {
  SType arg_stype = arg.stype();
  if (arg_stype == SType::STR32 || arg_stype == SType::STR64) {
    throw _error("sd", arg_stype);
  }
  SType res_stype = (arg_stype == SType::FLOAT32)? SType::FLOAT32
                                                 : SType::FLOAT64;
  if (arg.nrows() == 0) {
    return Column::new_na_column(1, res_stype);
  }
  return Column(new SdGrouped_ColumnImpl(res_stype, std::move(arg), gby));
}




//------------------------------------------------------------------------------
// count(A)
//------------------------------------------------------------------------------

#if 0
static const char* doc_count =
R"(count(cols)
--

Calculate the number of non-missing values for each column from `cols`.

Parameters
----------
cols: Expr
    Input columns.

return: Expr
    f-expression having one row, and the same names and number of columns
    as in `cols`. All the returned column stypes are `int64`.

except: TypeError
    The exception is raised when one of the columns from `cols`
    has a non-numeric and non-string type.

See Also
--------

- :func:`sum()` -- function to calculate the sum of values.

Examples
--------

.. code-block:: python

    >>> from datatable import dt, f
    >>>
    >>> df = dt.Frame({'A': [1, 1, 2, 1, 2],
    ...                'B': [None, 2, 3,4, 5],
    ...                'C': [1, 2, 1, 1, 2]})
    >>> df
       |     A      B      C
       | int32  int32  int32
    -- + -----  -----  -----
     0 |     1     NA      1
     1 |     1      2      2
     2 |     2      3      1
     3 |     1      4      1
     4 |     2      5      2
    [5 rows x 3 columns]

Get the count of all rows::

    >>> df[:, dt.count()]
       | count
       | int32
    -- + -----
     0 |     5
    [1 row x 1 column]

Get the count of column `B` (note how the null row is excluded from the
count result)::

    >>> df[:, dt.count(f.B)]
       |     B
       | int64
    -- + -----
     0 |     4
    [1 row x 1 column]
)";
#endif

template <typename T>
bool count_reducer(const Column& col, size_t i0, size_t i1, int64_t* out) {
  int64_t count = 0;
  for (size_t i = i0; i < i1; ++i) {
    T value;
    bool isvalid = col.get_element(i, &value);
    count += isvalid;
  }
  *out = count;
  return true;  // *out is not NA
}



template <typename T>
static Column _count(Column&& arg, const Groupby& gby) {
  return Column(
          new Latent_ColumnImpl(
            new Reduced_ColumnImpl<T, int64_t>(
                 SType::INT64, std::move(arg), gby, count_reducer<T>
            )));
}

static Column compute_count(Column&& arg, const Groupby& gby) {
  switch (arg.stype()) {
    case SType::BOOL:
    case SType::INT8:    return _count<int8_t>(std::move(arg), gby);
    case SType::INT16:   return _count<int16_t>(std::move(arg), gby);
    case SType::DATE32:
    case SType::INT32:   return _count<int32_t>(std::move(arg), gby);
    case SType::TIME64:
    case SType::INT64:   return _count<int64_t>(std::move(arg), gby);
    case SType::FLOAT32: return _count<float>(std::move(arg), gby);
    case SType::FLOAT64: return _count<double>(std::move(arg), gby);
    case SType::STR32:
    case SType::STR64:   return _count<CString>(std::move(arg), gby);
    default: throw _error("count", arg.stype());
  }
}


//------------------------------------------------------------------------------
// countna
//------------------------------------------------------------------------------


template <typename T>
bool op_countna(const Column& col, size_t i0, size_t i1, int64_t* out) {
  int64_t count = 0;
  for (size_t i = i0; i < i1; ++i) {
    T value;
    bool isvalid = col.get_element(i, &value);
    count += !isvalid;
  }
  *out = count;
  return true;  // *out is not NA
}



template <typename T>
static Column _countna(Column&& arg, const Groupby& gby) {
  return Column(
          new Latent_ColumnImpl(
            new Reduced_ColumnImpl<T, int64_t>(
                 SType::INT64, std::move(arg), gby, op_countna<T>
            )));
}

static Column compute_countna(Column&& arg, const Groupby& gby) {
  switch (arg.stype()) {
    case SType::BOOL:
    case SType::INT8:    return _countna<int8_t>(std::move(arg), gby);
    case SType::INT16:   return _countna<int16_t>(std::move(arg), gby);
    case SType::DATE32:
    case SType::INT32:   return _countna<int32_t>(std::move(arg), gby);
    case SType::TIME64:
    case SType::INT64:   return _countna<int64_t>(std::move(arg), gby);
    case SType::FLOAT32: return _countna<float>(std::move(arg), gby);
    case SType::FLOAT64: return _countna<double>(std::move(arg), gby);
    case SType::STR32:
    case SType::STR64:   return _countna<CString>(std::move(arg), gby);
    default: throw _error("countna", arg.stype());
  }
}


//------------------------------------------------------------------------------
// count(A:grouped)
//------------------------------------------------------------------------------

// T is the type of the input column
template <typename T>
class CountGrouped_ColumnImpl : public Virtual_ColumnImpl
{
  private:
    Column arg;
    Groupby groupby;

  public:
    CountGrouped_ColumnImpl(Column&& col, const Groupby& grpby)
      : Virtual_ColumnImpl(grpby.size(), SType::INT64),
        arg(std::move(col)),
        groupby(grpby) {}

    ColumnImpl* clone() const override {
      return new CountGrouped_ColumnImpl<T>(Column(arg), groupby);
    }

    bool get_element(size_t i, int64_t* out) const override {
      T value;
      bool isvalid = arg.get_element(i, &value);
      if (isvalid) {
        size_t i0, i1;
        groupby.get_group(i, &i0, &i1);
        *out = static_cast<int64_t>(i1 - i0);
      } else {
        *out = 0;
      }
      return true;
    }

    size_t n_children() const noexcept override {
      return 1;
    }

    const Column& child(size_t i) const override {
      xassert(i == 0);  (void)i;
      return arg;
    }

};


template <typename T>
static Column _gcount(Column&& arg, const Groupby& gby) {
  return Column(new CountGrouped_ColumnImpl<T>(std::move(arg), gby));
}

static Column compute_gcount(Column&& arg, const Groupby& gby) {
  switch (arg.stype()) {
    case SType::BOOL:
    case SType::INT8:    return _gcount<int8_t>(std::move(arg), gby);
    case SType::INT16:   return _gcount<int16_t>(std::move(arg), gby);
    case SType::DATE32:
    case SType::INT32:   return _gcount<int32_t>(std::move(arg), gby);
    case SType::TIME64:
    case SType::INT64:   return _gcount<int64_t>(std::move(arg), gby);
    case SType::FLOAT32: return _gcount<float>(std::move(arg), gby);
    case SType::FLOAT64: return _gcount<double>(std::move(arg), gby);
    case SType::STR32:
    case SType::STR64:   return _gcount<CString>(std::move(arg), gby);
    default: throw _error("count", arg.stype());
  }
}




//------------------------------------------------------------------------------
// min(A), max(A)
//------------------------------------------------------------------------------
#if 0
static const char* doc_min =
R"(min(cols)
--

Calculate the minimum value for each column from `cols`. It is recommended
to use it as `dt.min()` to prevent conflict with the Python built-in
`min()` function.

Parameters
----------
cols: Expr
    Input columns.

return: Expr
    f-expression having one row and the same names, stypes and number
    of columns as in `cols`.

except: TypeError
    The exception is raised when one of the columns from `cols`
    has a non-numeric type.


Examples
--------
.. code-block:: python

    >>> from datatable import dt, f, by
    >>>
    >>> df = dt.Frame({'A': [1, 1, 1, 2, 2, 2, 3, 3, 3],
    ...                'B': [3, 2, 20, 1, 6, 2, 3, 22, 1]})
    >>>
    >>> df
       |     A      B
       | int32  int32
    -- + -----  -----
     0 |     1      3
     1 |     1      2
     2 |     1     20
     3 |     2      1
     4 |     2      6
     5 |     2      2
     6 |     3      3
     7 |     3     22
     8 |     3      1
    [9 rows x 2 columns]

Get the minimum from column B::

    >>> df[:, dt.min(f.B)]
       |     B
       | int32
    -- + -----
     0 |     1
    [1 row x 1 column]

Get the minimum of all columns::

    >>> df[:, [dt.min(f.A), dt.min(f.B)]]
       |     A      B
       | int32  int32
    -- + -----  -----
     0 |     1      1
    [1 row x 2 columns]

Same as above, but using the slice notation::

    >>> df[:, dt.min(f[:])]
       |     A      B
       | int32  int32
    -- + -----  -----
     0 |     1      1
    [1 row x 2 columns]

In the presence of :func:`by()`, it returns the row with the minimum value
per group::

    >>> df[:, dt.min(f.B), by("A")]
       |     A      B
       | int32  int32
    -- + -----  -----
     0 |     1      2
     1 |     2      1
     2 |     3      1
    [3 rows x 2 columns]


See Also
--------
- :func:`max()` -- function to calculate maxium values.
)";


static const char* doc_max =
R"(max(cols)
--

Calculate the maximum value for each column from `cols`. It is recommended
to use it as `dt.max()` to prevent conflict with the Python built-in
`max()` function.

Parameters
----------
cols: Expr
    Input columns.

return: Expr
    f-expression having one row and the same names, stypes and number
    of columns as in `cols`.

except: TypeError
    The exception is raised when one of the columns from `cols`
    has a non-numeric type.


Examples
--------
.. code-block:: python

    >>> from datatable import dt, f, by
    >>>
    >>> df = dt.Frame({'A': [1, 1, 1, 2, 2, 2, 3, 3, 3],
    ...                'B': [3, 2, 20, 1, 6, 2, 3, 22, 1]})
    >>> df
       |     A      B
       | int32  int32
    -- + -----  -----
     0 |     1      3
     1 |     1      2
     2 |     1     20
     3 |     2      1
     4 |     2      6
     5 |     2      2
     6 |     3      3
     7 |     3     22
     8 |     3      1
    [9 rows x 2 columns]

Get the maximum from column B::

    >>> df[:, dt.max(f.B)]
       |     B
       | int32
    -- + -----
     0 |    22
    [1 row x 1 column]

Get the maximum of all columns::

    >>> df[:, [dt.max(f.A), dt.max(f.B)]]
       |     A      B
       | int32  int32
    -- + -----  -----
     0 |     3     22
    [1 row x 2 columns]

Same as above, but more convenient::

    >>> df[:, dt.max(f[:])]
       |     A      B
       | int32  int32
    -- + -----  -----
     0 |     3     22
    [1 row x 2 columns]

In the presence of :func:`by()`, it returns the row with the maximum
value per group::

    >>> df[:, dt.max(f.B), by("A")]
       |     A      B
       | int32  int32
    -- + -----  -----
     0 |     1     20
     1 |     2      6
     2 |     3     22
    [3 rows x 2 columns]


See Also
--------
- :func:`min()` -- function to calculate minimum values.
)";
#endif

template <typename T, bool MIN>
bool minmax_reducer(const Column& col, size_t i0, size_t i1, T* out) {
  T minmax = 0;
  bool minmax_isna = true;
  for (size_t i = i0; i < i1; ++i) {
    T value;
    bool isvalid = col.get_element(i, &value);
    if (!isvalid) continue;
    if ((MIN? (value < minmax) : (value > minmax)) || minmax_isna) {
      minmax = value;
      minmax_isna = false;
    }
  }
  *out = minmax;
  return !minmax_isna;
}



template <typename T, bool MM>
static Column _minmax(SType stype, Column&& arg, const Groupby& gby) {
  return Column(
          new Latent_ColumnImpl(
            new Reduced_ColumnImpl<T, T>(
                 stype, std::move(arg), gby, minmax_reducer<T, MM>
            )));
}

template <bool MIN>
static Column compute_minmax(Column&& arg, const Groupby& gby) {
  auto st = arg.stype();
  switch (st) {
    case SType::BOOL:
    case SType::INT8:    return _minmax<int8_t, MIN>(st, std::move(arg), gby);
    case SType::INT16:   return _minmax<int16_t, MIN>(st, std::move(arg), gby);
    case SType::DATE32:
    case SType::INT32:   return _minmax<int32_t, MIN>(st, std::move(arg), gby);
    case SType::TIME64:
    case SType::INT64:   return _minmax<int64_t, MIN>(st, std::move(arg), gby);
    case SType::FLOAT32: return _minmax<float, MIN>(st, std::move(arg), gby);
    case SType::FLOAT64: return _minmax<double, MIN>(st, std::move(arg), gby);
    default: throw _error(MIN? "min" : "max", arg.stype());
  }
}




//------------------------------------------------------------------------------
// Median
//------------------------------------------------------------------------------

#if 0
static const char* doc_median =
R"(median(cols)
--

Calculate the median value for each column from `cols`.

Parameters
----------
cols: Expr
    Input columns.

return: Expr
    f-expression having one row, and the same names, stypes and
    number of columns as in `cols`.

except: TypeError
    The exception is raised when one of the columns from `cols`
    has a non-numeric type.

See Also
--------

- :func:`mean()` -- function to calculate mean values.
- :func:`sd()` -- function to calculate standard deviation.

Examples
--------
.. code-block:: python

    >>> from datatable import dt, f, by
    >>>
    >>> df = dt.Frame({'A': [1, 1, 2, 1, 2],
    ...                'B': [None, 2, 3,4, 5],
    ...                'C': [1, 2, 1, 1, 2]})
    >>>
    >>> df
       |     A      B      C
       | int32  int32  int32
    -- + -----  -----  -----
     0 |     1     NA      1
     1 |     1      2      2
     2 |     2      3      1
     3 |     1      4      1
     4 |     2      5      2
    [5 rows x 3 columns]

Get the median from column A::

    >>> df[:, dt.median(f.A)]
       |       A
       | float64
    -- + -------
     0 |       1
    [1 row x 1 column]

Get the median of multiple columns::

    >>> df[:, dt.median([f.A, f.B])]
       |       A        B
       | float64  float64
    -- + -------  -------
     0 |       1      3.5
    [1 row x 2 columns]

Same as above, but more convenient::

    >>> df[:, dt.median(f[:2])]
       |       A        B
       | float64  float64
    -- + -------  -------
     0 |       1      3.5
    [1 row x 2 columns]

You can pass in a dictionary with new column names::

    >>> df[:, dt.median({"A_median": f.A, "C_mid": f.C})]
       | A_median    C_mid
       |  float64  float64
    -- + --------  -------
     0 |        1        1
    [1 row x 2 columns]

In the presence of :func:`by()`, it returns the median of each column
per group::

    >>> df[:, dt.median({"A_median": f.A, "B_median": f.B}), by("C")]
       |     C  A_median  B_median
       | int32   float64   float64
    -- + -----  --------  --------
     0 |     1       1         3.5
     1 |     2       1.5       3.5
    [2 rows x 3 columns]
)";
#endif


template <typename T, typename U>
class Median_ColumnImpl : public Virtual_ColumnImpl {
  private:
    Column arg;
    Groupby groupby;

  public:
    Median_ColumnImpl(Column&& col, const Groupby& grpby)
      : Virtual_ColumnImpl(grpby.size(), stype_from<U>),
        arg(std::move(col)),
        groupby(grpby) {}

    ColumnImpl* clone() const override {
      auto res = new Median_ColumnImpl<T, U>(Column(arg), groupby);
      return res;
    }

    void pre_materialize_hook() override {
      arg.sort_grouped(groupby);
    }

    bool get_element(size_t i, U* out) const override {
      size_t i0, i1;
      T value1, value2;
      groupby.get_group(i, &i0, &i1);
      xassert(i0 < i1);

      // skip NA values if any
      while (true) {
        bool isvalid = arg.get_element(i0, &value1);
        if (isvalid) break;
        ++i0;
        if (i0 == i1) return false;  // all elements are NA
      }

      size_t j = (i0 + i1) / 2;
      arg.get_element(j, &value1);
      if ((i1 - i0) & 1) { // Odd count of elements
        *out = static_cast<U>(value1);
      } else {
        arg.get_element(j-1, &value2);
        *out = (static_cast<U>(value1) + static_cast<U>(value2))/2;
      }
      return true;
    }


    size_t n_children() const noexcept override {
      return 1;
    }

    const Column& child(size_t i) const override {
      xassert(i == 0);  (void)i;
      return arg;
    }
};


template <typename T>
static Column _median(Column&& arg, const Groupby& gby) {
  using U = typename std::conditional<std::is_same<T, float>::value,
                                      float, double>::type;
  return Column(
          new Latent_ColumnImpl(
            new Median_ColumnImpl<T, U>(std::move(arg), gby)
          ));
}

static Column compute_median(Column&& arg, const Groupby& gby) {
  if (arg.nrows() == 0) {
    return Column::new_na_column(1, arg.stype());
  }
  switch (arg.stype()) {
    case SType::BOOL:
    case SType::INT8:    return _median<int8_t> (std::move(arg), gby);
    case SType::INT16:   return _median<int16_t>(std::move(arg), gby);
    case SType::INT32:   return _median<int32_t>(std::move(arg), gby);
    case SType::INT64:   return _median<int64_t>(std::move(arg), gby);
    case SType::FLOAT32: return _median<float>  (std::move(arg), gby);
    case SType::FLOAT64: return _median<double> (std::move(arg), gby);
    default: throw _error("median", arg.stype());
  }
}


static Column compute_gmedian(Column&& arg, const Groupby&) {
  SType arg_stype = arg.stype();
  if (arg_stype == SType::STR32 || arg_stype == SType::STR64) {
    throw _error("median", arg_stype);
  }
  SType res_stype = (arg_stype == SType::FLOAT32)? SType::FLOAT32
                                                 : SType::FLOAT64;
  if (arg.nrows() == 0) {
    return Column::new_na_column(1, res_stype);
  }
  arg.cast_inplace(res_stype);
  return std::move(arg);
}





//------------------------------------------------------------------------------
// Head_Reduce_Unary
//------------------------------------------------------------------------------

Workframe Head_Reduce_Unary::evaluate_n(
    const vecExpr& args, EvalContext& ctx) const
{
  xassert(args.size() == 1);
  Workframe inputs = args[0]->evaluate_n(ctx);
  Groupby gby = ctx.get_groupby();
  if (!gby) gby = Groupby::single_group(ctx.nrows());

  maker_fn fn = nullptr;
  if (inputs.get_grouping_mode() == Grouping::GtoALL) {
    switch (op) {
      case Op::MEAN:   fn = compute_mean; break;
      case Op::MIN:    fn = compute_minmax<true>; break;
      case Op::MAX:    fn = compute_minmax<false>; break;
      case Op::STDEV:  fn = compute_sd; break;
      case Op::FIRST:  fn = compute_firstlast<true>; break;
      case Op::LAST:   fn = compute_firstlast<false>; break;
      case Op::SUM:    fn = compute_sum; break;
      case Op::COUNT:  fn = compute_count; break;
      case Op::MEDIAN: fn = compute_median; break;
      case Op::COUNTNA:fn = compute_countna; break;
      default: throw TypeError() << "Unknown reducer function: "
                                 << static_cast<size_t>(op);
    }
  } else {
    switch (op) {
      case Op::MEAN:   fn = compute_gmean; break;
      case Op::STDEV:  fn = compute_gsd; break;
      case Op::MIN:
      case Op::MAX:
      case Op::FIRST:
      case Op::LAST:   fn = compute_gfirstlast; break;
      case Op::SUM:    fn = compute_gsum; break;
      case Op::COUNT:  fn = compute_gcount; break;
      case Op::MEDIAN: fn = compute_gmedian; break;
      default: throw TypeError() << "Unknown reducer function: "
                                 << static_cast<size_t>(op);
    }
  }

  Workframe outputs(ctx);
  for (size_t i = 0; i < inputs.ncols(); ++i) {
    outputs.add_column(
        fn(inputs.retrieve_column(i), gby),
        inputs.retrieve_name(i),
        Grouping::GtoONE);
  }
  return outputs;
}




}}  // namespace dt::expr
