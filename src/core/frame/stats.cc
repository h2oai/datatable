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
    The frame will have 1 row and the same number and names of columns
    as in the current frame. All columns will have stype ``int64``.

Examples
--------
>>> DT = dt.Frame(A=[1, 5, None], B=[math.nan]*3, C=[None, None, 'bah!'])
>>> DT.countna()
   |     A      B      C
   | int64  int64  int64
-- + -----  -----  -----
 0 |     1      3      2
--
[1 row x 3 columns]

>>> DT.countna().to_tuples()[0]
(1, 3, 2)


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
    If called on a Frame that has more or less than 1 column.

(return): int

See Also
--------
- :meth:`.countna()` -- similar to this method, but can be applied to
  a Frame with any number of columns.

- :func:`dt.count()` -- function for counting non-NA ("valid") values
  in a column; can also be applied per-group.
)";


static const char* doc_max =
R"(max(self)
--

Report the largest (maximum) value in each column of the frame.

Parameters
----------
return: Frame
    The frame will have 1 row and the same number, names and stypes
    of columns as in the current frame. For string/object columns
    this function returns NA values.

See Also
--------
- :meth:`.max1()` -- similar to this method, but operates on a
  single-column frame only, and returns a scalar value instead of
  a Frame.

- :func:`dt.max()` -- function for counting max values in a column or
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
    If called on a Frame that has more or less than 1 column.

See Also
--------
- :meth:`.max()` -- similar to this method, but can be applied to
  a Frame with any number of columns.

- :func:`dt.max()` -- function for counting max values in a column or
  an expression; can also be applied per-group.
)";


static const char* doc_min =
R"(min(self)
--

Report the smallest (minimum) value in each column of the frame.

Parameters
----------
return: Frame
    The frame will have 1 row and the same number, names and stypes
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

Return the smallest value in a single-column Frame. The frame's
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
  a Frame with any number of columns.

- :func:`dt.min()` -- function for counting min values in a column or
  an expression; can also be applied per-group.
)";




static PKArgs args_countna(0, 0, 0, false, false, {}, "countna", doc_countna);
static PKArgs args_max(0, 0, 0, false, false, {}, "max", nullptr);
static PKArgs args_mean(0, 0, 0, false, false, {}, "mean", nullptr);
static PKArgs args_min(0, 0, 0, false, false, {}, "min", nullptr);
static PKArgs args_mode(0, 0, 0, false, false, {}, "mode", nullptr);
static PKArgs args_nmodal(0, 0, 0, false, false, {}, "nmodal", nullptr);
static PKArgs args_nunique(0, 0, 0, false, false, {}, "nunique", nullptr);
static PKArgs args_sd(0, 0, 0, false, false, {}, "sd", nullptr);
static PKArgs args_sum(0, 0, 0, false, false, {}, "sum", nullptr);

oobj Frame::stat(const PKArgs& args) {
  Stat stat = stat_from_args[&args];
  DataTable* res = _make_frame(dt, stat);
  return Frame::oframe(res);
}


static PKArgs args_countna1(0, 0, 0, false, false, {}, "countna1", doc_countna1);
static PKArgs args_mean1(0, 0, 0, false, false, {}, "mean1", nullptr);
static PKArgs args_sd1(0, 0, 0, false, false, {}, "sd1", nullptr);
static PKArgs args_min1(0, 0, 0, false, false, {}, "min1", nullptr);
static PKArgs args_max1(0, 0, 0, false, false, {}, "max1", nullptr);
static PKArgs args_mode1(0, 0, 0, false, false, {}, "mode1", nullptr);
static PKArgs args_nmodal1(0, 0, 0, false, false, {}, "nmodal1", nullptr);
static PKArgs args_nunique1(0, 0, 0, false, false, {}, "nunique1", nullptr);
static PKArgs args_sum1(0, 0, 0, false, false, {}, "sum1", nullptr);

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
