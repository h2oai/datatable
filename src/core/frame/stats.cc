//------------------------------------------------------------------------------
// Copyright 2018-2020 H2O.ai
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
#include <cstdint>
#include <type_traits>
#include <unordered_map>
#include "frame/py_frame.h"
#include "python/_all.h"
#include "python/string.h"
#include "column.h"
#include "buffer.h"
#include "stats.h"

namespace py {


//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

static std::unordered_map<const PKArgs*, Stat> stat_from_args;

static DataTable* _make_frame(DataTable* dt, Stat stat) {
  colvec out_cols;
  out_cols.reserve(dt->ncols());
  for (size_t i = 0; i < dt->ncols(); ++i) {
    const Column& dtcol = dt->get_column(i);
    out_cols.push_back(dtcol.stats()->get_stat_as_column(stat));
  }
  return new DataTable(std::move(out_cols), *dt);
}



//------------------------------------------------------------------------------
// Frame functions
//------------------------------------------------------------------------------

static const char* doc_countna =
R"(countna(self)
--

Report the number of NA values in each column of the frame.

Parameters
----------
(return): Frame
    The frame will have one row and the same number/names of columns
    as in the current frame. All columns will have stype ``int64``.

Examples
--------
.. code-block:: python

    >>> DT = dt.Frame(A=[1, 5, None], B=[math.nan]*3, C=[None, None, 'bah!'])
    >>> DT.countna()
       |     A      B      C
       | int64  int64  int64
    -- + -----  -----  -----
     0 |     1      3      2
    [1 row x 3 columns]

    >>> DT.countna().to_tuples()[0]
    >>> (1, 3, 2)


See Also
--------
- :meth:`.countna1()` -- similar to this method, but operates on a
  single-column frame only, and returns a number instead of a Frame.

- :func:`dt.count()` -- function for counting non-NA ("valid") values
  in a column; can also be applied per-group.
)";


static const char* doc_countna1 =
R"(countna1(self)
--

Return the number of NA values in a single-column Frame.

This function is a shortcut for::

    DT.countna()[0, 0]

Parameters
----------
(except): ValueError
    If called on a Frame that has more or less than one column.

(return): int

See Also
--------
- :meth:`.countna()` -- similar to this method, but can be applied to
  a Frame with an arbitrary number of columns.

- :func:`dt.count()` -- function for counting non-NA ("valid") values
  in a column; can also be applied per-group.
)";


static const char* doc_max =
R"(max(self)
--

Find the largest value in each column of the frame.

Parameters
----------
return: Frame
    The frame will have one row and the same number, names and stypes
    of columns as in the current frame. For string/object columns
    this function returns NA values.

See Also
--------
- :meth:`.max1()` -- similar to this method, but operates on a
  single-column frame only, and returns a scalar value instead of
  a Frame.

- :func:`dt.max()` -- function for finding largest values in a column or
  an expression; can also be applied per-group.
)";


static const char* doc_max1 =
R"(max1(self)
--

Return the largest value in a single-column Frame. The frame's
stype must be numeric.

This function is a shortcut for::

    DT.max()[0, 0]

Parameters
----------
return: bool | int | float
    The returned value corresponds to the stype of the frame.

except: ValueError
    If called on a Frame that has more or less than one column.

See Also
--------
- :meth:`.max()` -- similar to this method, but can be applied to
  a Frame with an arbitrary number of columns.

- :func:`dt.max()` -- function for counting max values in a column or
  an expression; can also be applied per-group.
)";


static const char* doc_min =
R"(min(self)
--

Find the smallest value in each column of the frame.

Parameters
----------
return: Frame
    The frame will have one row and the same number, names and stypes
    of columns as in the current frame. For string/object columns
    this function returns NA values.

See Also
--------
- :meth:`.min1()` -- similar to this method, but operates on a
  single-column frame only, and returns a scalar value instead of
  a Frame.

- :func:`dt.min()` -- function for counting min values in a column or
  an expression; can also be applied per-group.
)";


static const char* doc_min1 =
R"(min1(self)
--

Find the smallest value in a single-column Frame. The frame's
stype must be numeric.

This function is a shortcut for::

    DT.min()[0, 0]

Parameters
----------
return: bool | int | float
    The returned value corresponds to the stype of the frame.

except: ValueError
    If called on a Frame that has more or less than 1 column.

See Also
--------
- :meth:`.min()` -- similar to this method, but can be applied to
  a Frame with an arbitrary number of columns.

- :func:`dt.min()` -- function for counting min values in a column or
  an expression; can also be applied per-group.
)";


static const char* doc_mean =
R"(mean(self)
--

Calculate the mean value for each column in the frame.

Parameters
----------
return: Frame
    The frame will have one row and the same number/names
    of columns as in the current frame. All columns will have `float64`
    stype. For string/object columns this function returns NA values.

See Also
--------
- :meth:`.mean1()` -- similar to this method, but operates on a
  single-column frame only, and returns a scalar value instead of
  a Frame.

- :func:`dt.mean()` -- function for counting mean values in a column or
  an expression; can also be applied per-group.
)";


static const char* doc_mean1 =
R"(mean1(self)
--

Calculate the mean value for a single-column Frame.

This function is a shortcut for::

    DT.mean()[0, 0]

Parameters
----------
return: None | float
    `None` is returned for string/object columns.

except: ValueError
    If called on a Frame that has more or less than one column.

See Also
--------
- :meth:`.mean()` -- similar to this method, but can be applied to
  a Frame with an arbitrary number of columns.

- :func:`dt.mean()` -- function for calculatin mean values in a column or
  an expression; can also be applied per-group.
)";



static const char* doc_mode =
R"(mode(self)
--

Find the mode for each column in the frame.

Parameters
----------
return: Frame
    The frame will have one row and the same number/names
    of columns as in the current frame.

See Also
--------
- :meth:`.mode1()` -- similar to this method, but operates on a
  single-column frame only, and returns a scalar value instead of
  a Frame.

)";


static const char* doc_mode1 =
R"(mode1(self)
--

Find the mode for a single-column Frame.

This function is a shortcut for::

    DT.mode()[0, 0]

Parameters
----------
return: bool | int | float | str | object
    The returned value corresponds to the stype of the column.

except: ValueError
    If called on a Frame that has more or less than one column.

See Also
--------
- :meth:`.mode()` -- similar to this method, but can be applied to
  a Frame with an arbitrary number of columns.

)";


static const char* doc_nmodal =
R"(nmodal(self)
--

Calculate the modal frequency for each column in the frame.

Parameters
----------
return: Frame
    The frame will have one row and the same number/names
    of columns as in the current frame. All the columns
    will have `int64` stype.

See Also
--------
- :meth:`.nmodal1()` -- similar to this method, but operates on a
  single-column frame only, and returns a scalar value instead of
  a Frame.

)";


static const char* doc_nmodal1 =
R"(nmodal1(self)
--

Calculate the modal frequency for a single-column Frame.

This function is a shortcut for::

    DT.nmodal()[0, 0]

Parameters
----------
return: int

except: ValueError
    If called on a Frame that has more or less than one column.

See Also
--------
- :meth:`.nmodal()` -- similar to this method, but can be applied to
  a Frame with an arbitrary number of columns.

)";


static const char* doc_nunique =
R"(nunique(self)
--

Count the number of unique values for each column in the frame.

Parameters
----------
return: Frame
    The frame will have one row and the same number/names
    of columns as in the current frame. All the columns
    will have `int64` stype.

See Also
--------
- :meth:`.nunique1()` -- similar to this method, but operates on a
  single-column frame only, and returns a scalar value instead of
  a Frame.

)";


static const char* doc_nunique1 =
R"(nunique1(self)
--

Count the number of unique values for a one-column frame and return it as a scalar.

This function is a shortcut for::

    DT.nunique()[0, 0]

Parameters
----------
return: int

except: ValueError
    If called on a Frame that has more or less than one column.

See Also
--------
- :meth:`.nunique()` -- similar to this method, but can be applied to
  a Frame with an arbitrary number of columns.

)";


static const char* doc_sd =
R"(sd(self)
--

Calculate the standard deviation for each column in the frame.

Parameters
----------
return: Frame
    The frame will have one row and the same number/names
    of columns as in the current frame. All the columns
    will have `float64` stype. For non-numeric columns
    this function returns NA values.

See Also
--------
- :meth:`.sd1()` -- similar to this method, but operates on a
  single-column frame only, and returns a scalar value instead of
  a Frame.


- :func:`dt.sd()` -- function for calculating the standard deviation
  in a column or an expression; can also be applied per-group.

)";


static const char* doc_sd1 =
R"(sd1(self)
--

Calculate the standard deviation for a one-column frame and
return it as a scalar.

This function is a shortcut for::

    DT.sd()[0, 0]

Parameters
----------
return: None | float
    `None` is returned for non-numeric columns.

except: ValueError
    If called on a Frame that has more or less than one column.

See Also
--------
- :meth:`.sd()` -- similar to this method, but can be applied to
  a Frame with an arbitrary number of columns.

- :func:`dt.sd()` -- function for calculating the standard deviation
  in a column or an expression; can also be applied per-group.

)";


static const char* doc_sum =
R"(sum(self)
--

Calculate the sum of all values for each column in the frame.

Parameters
----------
return: Frame
    The frame will have one row and the same number/names
    of columns as in the current frame. All the columns
    will have `float64` stype. For non-numeric columns
    this function returns NA values.

See Also
--------
- :meth:`.sum1()` -- similar to this method, but operates on a
  single-column frame only, and returns a scalar value instead of
  a Frame.


- :func:`dt.sum()` -- function for calculating the sum of all the values
  in a column or an expression; can also be applied per-group.

)";


static const char* doc_sum1 =
R"(sum1(self)
--

Calculate the sum of all values for a one-column column frame and
return it as a scalar.

This function is a shortcut for::

    DT.sum()[0, 0]

Parameters
----------
return: None | float
    `None` is returned for non-numeric columns.

except: ValueError
    If called on a Frame that has more or less than one column.

See Also
--------
- :meth:`.sum()` -- similar to this method, but can be applied to
  a Frame with an arbitrary number of columns.

- :func:`dt.sum()` -- function for calculating the sum of all the values
  in a column or an expression; can also be applied per-group.

)";



static PKArgs args_countna(0, 0, 0, false, false, {}, "countna", doc_countna);
static PKArgs args_max(0, 0, 0, false, false, {}, "max", doc_max);
static PKArgs args_mean(0, 0, 0, false, false, {}, "mean", doc_mean);
static PKArgs args_min(0, 0, 0, false, false, {}, "min", doc_min);
static PKArgs args_mode(0, 0, 0, false, false, {}, "mode", doc_mode);
static PKArgs args_nmodal(0, 0, 0, false, false, {}, "nmodal", doc_nmodal);
static PKArgs args_nunique(0, 0, 0, false, false, {}, "nunique", doc_nunique);
static PKArgs args_sd(0, 0, 0, false, false, {}, "sd", doc_sd);
static PKArgs args_sum(0, 0, 0, false, false, {}, "sum", doc_sum);

oobj Frame::stat(const PKArgs& args) {
  Stat stat = stat_from_args[&args];
  DataTable* res = _make_frame(dt, stat);
  return Frame::oframe(res);
}


static PKArgs args_countna1(0, 0, 0, false, false, {}, "countna1", doc_countna1);
static PKArgs args_max1(0, 0, 0, false, false, {}, "max1", doc_max1);
static PKArgs args_mean1(0, 0, 0, false, false, {}, "mean1", doc_mean1);
static PKArgs args_min1(0, 0, 0, false, false, {}, "min1", doc_min1);
static PKArgs args_mode1(0, 0, 0, false, false, {}, "mode1", doc_mode1);
static PKArgs args_nmodal1(0, 0, 0, false, false, {}, "nmodal1", doc_nmodal1);
static PKArgs args_nunique1(0, 0, 0, false, false, {}, "nunique1", doc_nunique1);
static PKArgs args_sd1(0, 0, 0, false, false, {}, "sd1", doc_sd1);
static PKArgs args_sum1(0, 0, 0, false, false, {}, "sum1", doc_sum1);

oobj Frame::stat1(const PKArgs& args) {
  if (dt->ncols() != 1) {
    throw ValueError() << "This method can only be applied to a 1-column Frame";
  }
  const Column& col0 = dt->get_column(0);
  Stat stat = stat_from_args[&args];
  return col0.stats()->get_stat_as_pyobject(stat);
}



void Frame::_init_stats(XTypeMaker& xt) {
  xt.add(METHOD(&Frame::stat, args_countna));
  xt.add(METHOD(&Frame::stat, args_sum));
  xt.add(METHOD(&Frame::stat, args_min));
  xt.add(METHOD(&Frame::stat, args_max));
  xt.add(METHOD(&Frame::stat, args_mode));
  xt.add(METHOD(&Frame::stat, args_mean));
  xt.add(METHOD(&Frame::stat, args_sd));
  xt.add(METHOD(&Frame::stat, args_nunique));
  xt.add(METHOD(&Frame::stat, args_nmodal));

  xt.add(METHOD(&Frame::stat1, args_countna1));
  xt.add(METHOD(&Frame::stat1, args_sum1));
  xt.add(METHOD(&Frame::stat1, args_mean1));
  xt.add(METHOD(&Frame::stat1, args_sd1));
  xt.add(METHOD(&Frame::stat1, args_min1));
  xt.add(METHOD(&Frame::stat1, args_max1));
  xt.add(METHOD(&Frame::stat1, args_mode1));
  xt.add(METHOD(&Frame::stat1, args_nmodal1));
  xt.add(METHOD(&Frame::stat1, args_nunique1));

  //---- Args -> Stat map ------------------------------------------------------

  stat_from_args[&args_countna] = Stat::NaCount;
  stat_from_args[&args_sum]     = Stat::Sum;
  stat_from_args[&args_mean]    = Stat::Mean;
  stat_from_args[&args_sd]      = Stat::StDev;
  stat_from_args[&args_min]     = Stat::Min;
  stat_from_args[&args_max]     = Stat::Max;
  stat_from_args[&args_mode]    = Stat::Mode;
  stat_from_args[&args_nmodal]  = Stat::NModal;
  stat_from_args[&args_nunique] = Stat::NUnique;

  stat_from_args[&args_countna1] = Stat::NaCount;
  stat_from_args[&args_sum1]     = Stat::Sum;
  stat_from_args[&args_mean1]    = Stat::Mean;
  stat_from_args[&args_sd1]      = Stat::StDev;
  stat_from_args[&args_min1]     = Stat::Min;
  stat_from_args[&args_max1]     = Stat::Max;
  stat_from_args[&args_mode1]    = Stat::Mode;
  stat_from_args[&args_nmodal1]  = Stat::NModal;
  stat_from_args[&args_nunique1] = Stat::NUnique;
}



}  // namespace py
