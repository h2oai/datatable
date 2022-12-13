#!/usr/bin/env python
#-------------------------------------------------------------------------------
# Copyright 2018-2022 H2O.ai
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
# IN THE SOFTWARE.
#-------------------------------------------------------------------------------
import re as base_re

from .frame import Frame
from .expr import (mean, min, max, sd, isna, sum, count, first, abs, exp,
                   last, log, log10, f, g, median, cov, corr, countna, nunique, prod)
from .lib._datatable import (
    as_type,
    by,
    categories,
    cbind,
    codes,
    cumcount,
    cummax,
    cummin,
    cumprod,
    cumsum,
    cut,
    FExpr,
    fillna,
    fread,
    ifelse,
    init_styles,
    intersect,
    iread,
    join,
    Namespace,
    ngroup,
    qcut,
    rbind,
    repeat,
    rowall,
    rowany,
    rowcount,
    rowfirst,
    rowlast,
    rowargmax,
    rowargmin,
    rowmax,
    rowmean,
    rowmin,
    rowsd,
    rowsum,
    setdiff,
    shift,
    sort,
    split_into_nhot_deprecated as split_into_nhot,
    symdiff,
    Type,
    union,
    unique,
    update,
)
from .types import stype, ltype
import datatable.math
import datatable.internal
import datatable.exceptions
import datatable.options
import datatable.re
import datatable.str
import datatable.time
try:
    from ._build_info import build_info
    __version__ = build_info.version  # pypi incompatible
except ImportError:
    __version__ = ""


__all__ = (
    "as_type",
    "bool8",
    "by",
    "categories",
    "cbind",
    "codes",
    "corr",
    "count",
    "cov",
    "cumcount",
    "cummax",
    "cummin",
    "cumprod",
    "cumsum",
    "cut",
    "dt",
    "exp",
    "FExpr",
    "f",
    "fillna",
    "first",
    "float32",
    "float64",
    "Frame",
    "fread",
    "g",
    "ifelse",
    "init_styles",
    "int16",
    "int32",
    "int64",
    "int8",
    "intersect",
    "iread",
    "isna",
    "join",
    "last",
    "log",
    "log10",
    "ltype",
    "mean",
    "median",
    "ngroup",
    "obj64",
    "options",
    "qcut",
    "rbind",
    "repeat",
    "rowall",
    "rowany",
    "rowcount",
    "rowfirst",
    "rowlast",
    "rowargmax",
    "rowargmin",
    "rowmax",
    "rowmean",
    "rowmin",
    "rowsd",
    "rowsum",
    "sd",
    "setdiff",
    "shift",
    "sort",
    "split_into_nhot",
    "str32",
    "str64",
    "stype",
    "symdiff",
    "Type",
    "union",
    "unique",
)

void = stype.void
bool8 = stype.bool8
int8 = stype.int8
int16 = stype.int16
int32 = stype.int32
int64 = stype.int64
float32 = stype.float32
float64 = stype.float64
str32 = stype.str32
str64 = stype.str64
obj64 = stype.obj64
dt = datatable
del datatable

# This will run only in Jupyter notebook
init_styles()

options = dt.options.Config(options={}, prefix="")
dt.lib._datatable.initialize_options(options)
dt.lib._datatable.initialize_final()
