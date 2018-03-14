#!/usr/bin/env python
# © H2O.ai 2018; -*- encoding: utf-8 -*-
#   This Source Code Form is subject to the terms of the Mozilla Public
#   License, v. 2.0. If a copy of the MPL was not distributed with this
#   file, You can obtain one at http://mozilla.org/MPL/2.0/.
#-------------------------------------------------------------------------------
import pytest
import datatable as dt
import random
import statistics
from datatable import stype, ltype
from math import inf, nan, isnan
from tests import list_equals


srcs_bool = [(False, True, False, False, True),
             (True, None, None, True, False)]
srcs_int = [(5, -3, 6, 3, 0),
            (None, -1, 0, 26, -3),
            (129, 38, 27, -127, 8),
            (385, None, None, -3, -89),
            (-192, 32769, 683, 94, 0),
            (None, -32788, -4, -44444, 5),
            (30, -284928, 59, 3, 2147483649),
            (2147483648, None, None, None, None)]
srcs_real = [(9.5, 0.2, 5.4857301, -3.14159265358979),
             (1.1, 2.3e12, -.5, None, inf, 0.0),
             (3.5, 2.36, nan, 696.9, 4097)]

srcs_str = [("foo", None, "bar", "baaz", None),
            ("a", "c", "d", None, "d", None, None, "a", "e", "c", "a", "a")]

srcs_numeric = srcs_bool + srcs_int + srcs_real
srcs_all = srcs_numeric + srcs_str


#-------------------------------------------------------------------------------
# Minimum function dt.min()
#-------------------------------------------------------------------------------

def t_min(t):
    t = [i for i in t if i is not None and not isnan(i)]
    if len(t) == 0:
        return None
    else:
        return min(t)


@pytest.mark.parametrize("src", srcs_numeric)
def test_min(src):
    dt0 = dt.Frame(src)
    dtr = dt0.min()
    assert dtr.internal.check()
    assert dtr.names == dt0.names
    assert dtr.stypes == dt0.stypes
    assert dtr.shape == (1, 1)
    assert dtr.topython() == [[t_min(src)]]
    assert dtr.scalar() == dt0.min1()


def test_dt_str():
    dt0 = dt.Frame([[1, 5, 3, 9, -2], list("abcde")])
    dtr = dt0.min()
    assert dtr.internal.check()
    assert dtr.topython() == [[-2], [None]]



#-------------------------------------------------------------------------------
# Maximum function dt.max()
#-------------------------------------------------------------------------------

def t_max(t):
    t = [i for i in t if i is not None and not isnan(i)]
    if len(t) == 0:
        return None
    else:
        return max(t)


@pytest.mark.parametrize("src", srcs_numeric)
def test_max(src):
    dt0 = dt.Frame(src)
    dtr = dt0.max()
    assert dtr.internal.check()
    assert dtr.names == dt0.names
    assert dtr.stypes == dt0.stypes
    assert dtr.shape == (1, 1)
    assert dtr.topython() == [[t_max(src)]]
    assert dtr.scalar() == dt0.max1()



#-------------------------------------------------------------------------------
# Sum function dt.sum()
#-------------------------------------------------------------------------------

def t_sum(t):
    t = [i for i in t if i is not None and not isnan(i)]
    if len(t) == 0:
        return 0
    else:
        return sum(t)

# Helper function that provides the result stype after `sum()` is called
def sum_stype(st):
    return stype.float64 if st.ltype == ltype.real else stype.int64


@pytest.mark.parametrize("src", srcs_numeric)
def test_sum(src):
    dt0 = dt.Frame(src)
    dtr = dt0.sum()
    assert dtr.internal.check()
    assert dtr.stypes == (sum_stype(dt0.stypes[0]), )
    assert dtr.shape == (1, dt0.ncols)
    assert dt0.names == dtr.names
    assert list_equals(dtr.topython(), [[t_sum(src)]])
    assert list_equals([dtr.scalar()], [dt0.sum1()])



#-------------------------------------------------------------------------------
# Mean function dt.mean()
#-------------------------------------------------------------------------------

def t_mean(t):
    t = [i for i in t if i is not None and not isnan(i)]
    if len(t) == 0:
        return None
    else:
        return statistics.mean(t)

@pytest.mark.parametrize("src", srcs_numeric)
def test_dt_mean(src):
    dt0 = dt.Frame(src)
    dtr = dt0.mean()
    assert dtr.internal.check()
    assert dt0.names == dtr.names
    assert dtr.stypes == (stype.float64,)
    assert dtr.shape == (1, 1)
    assert list_equals(dtr.topython(), [[t_mean(src)]])
    assert list_equals([dtr.scalar()], [dt0.mean1()])


@pytest.mark.parametrize("src, res", [([1, 3, 5, None], 3),
                                      ([], None),
                                      ([10], 10),
                                      ([1.5], 1.5),
                                      ([2, 4], 3),
                                      ([None, 1.3], 1.3),
                                      ([None, None, None], None),
                                      ([inf, inf, inf], inf),
                                      ([5, 1, inf], inf),
                                      ([inf, 4, 2], inf),
                                      ([-inf, -inf, -inf, -inf], -inf),
                                      ([-inf, 0, inf], None),
                                      ])
def test_dt_mean_special_cases(src, res):
    dt0 = dt.Frame(src)
    dtr = dt0.mean()
    assert dtr.internal.check()
    assert dt0.names == dtr.names
    assert dtr.stypes == (stype.float64,)
    assert dtr.shape == (1, 1)
    assert list_equals(dtr.topython(), [[res]])



#-------------------------------------------------------------------------------
# Standard Deviation function dt.sd()
#-------------------------------------------------------------------------------

def t_sd(t):
    t = [i for i in t
         if i is not None and not isnan(i)]
    if len(t) == 0:
        return None
    elif len(t) == 1:
        return 0
    else:
        res = statistics.stdev(t)
        return res if not isnan(res) else None

@pytest.mark.parametrize("src", srcs_numeric)
def test_dt_sd(src):
    dt0 = dt.Frame(src)
    dtr = dt0.sd()
    assert dtr.internal.check()
    assert dtr.stypes == (stype.float64, )
    assert dtr.shape == (1, 1)
    assert dt0.names == dtr.names
    assert list_equals(dtr.topython(), [[t_sd(src)]])
    assert list_equals([dtr.scalar()], [dt0.sd1()])


@pytest.mark.parametrize("src, res", [([1, 3, 5, None], 2.0),
                                      ([], None),
                                      ([3.0], 0),
                                      ([None, None, None], None),
                                      ([inf, inf, inf], None),
                                      ([-inf, 0, inf], None),
                                      ])
def test_dt_sd_special_cases(src, res):
    dt0 = dt.Frame(src)
    dtr = dt0.sd()
    assert dtr.internal.check()
    assert dtr.stypes == (stype.float64, )
    assert dtr.shape == (1, 1)
    assert dt0.names == dtr.names
    assert list_equals(dtr.topython(), [[res]])



#-------------------------------------------------------------------------------
# Count_na function dt.count_na()
#-------------------------------------------------------------------------------

def t_count_na(arr):
    return sum(x is None or (isinstance(x, float) and isnan(x))
               for x in arr)


@pytest.mark.parametrize("src", srcs_all)
def test_dt_count_na(src):
    dt0 = dt.Frame(src)
    dtr = dt0.countna()
    assert dtr.internal.check()
    assert dtr.stypes == (stype.int64, )
    assert dtr.shape == (1, 1)
    assert dt0.names == dtr.names
    assert dtr.topython() == [[t_count_na(src)]]
    assert dtr.scalar() == dt0.countna1()



#-------------------------------------------------------------------------------
# N_unique function
#-------------------------------------------------------------------------------

def n_unique(arr):
    return len(set(arr) - {None, nan})

@pytest.mark.parametrize("src", srcs_all)
def test_dt_n_unique(src):
    dt0 = dt.Frame(src)
    dtr = dt0.nunique()
    assert dtr.internal.check()
    assert dtr.stypes == (stype.int64, )
    assert dtr.shape == (1, 1)
    assert dtr.names == dt0.names
    assert dtr.scalar() == n_unique(src)
    assert dtr.scalar() == dt0.nunique1()



#-------------------------------------------------------------------------------
# Special cases
#-------------------------------------------------------------------------------

@pytest.mark.parametrize(
    "src, view, exp",
    [([-3, 6, 1, 0, 4], slice(2, 5), 0),
     ([-3, 6, 1, 0, 4], [1, 4], 4),
     ([3.5, -182, None, 2, 3], slice(2, None, 2), 3),
     ([3.5, -182, None, 2, 3], [0, 2, 4], 3),
     ([True, True, True, None, False], slice(4), True),
     ([True, True, True, None, False], [0, 3], True)])
def test_stats_on_view(src, view, exp):
    dt0 = dt.Frame(src)
    dt_view = dt0(view)
    res = dt_view.min()
    assert res.scalar() == exp


def test_bad_call():
    f0 = dt.Frame([range(10), range(10)])
    assert f0.ncols == 2
    with pytest.raises(ValueError) as e:
        f0.min1()
    assert "This method can only be applied to a 1-column Frame" in str(e.value)



#-------------------------------------------------------------------------------
# Statistics on large arrays
#-------------------------------------------------------------------------------

def test_stats_bool_large(numpy):
    n = 12345678
    a = numpy.random.randint(2, size=n, dtype=numpy.bool8)
    dt0 = dt.Frame(a)
    assert dt0.sum().topython() == [[a.sum()]]
    assert dt0.countna().topython() == [[0]]
    assert list_equals(dt0.mean().topython(), [[a.mean()]])
    assert list_equals(dt0.sd().topython(), [[a.std(ddof=1)]])


def test_stats_int_large(numpy):
    n = 12345678
    a = numpy.random.randint(2**20, size=n, dtype=numpy.int32)
    dt0 = dt.Frame(a)
    assert dt0.min().topython() == [[a.min()]]
    assert dt0.max().topython() == [[a.max()]]
    assert dt0.sum().topython() == [[a.sum()]]
    assert dt0.countna().topython() == [[0]]
    assert list_equals(dt0.mean().topython(), [[a.mean()]])
    assert list_equals(dt0.sd().topython(), [[a.std(ddof=1)]])


def test_stats_float_large(numpy):
    n = 12345678
    a = numpy.random.random(size=n) * 1e6
    dt0 = dt.Frame(a)
    assert dt0.min().topython() == [[a.min()]]
    assert dt0.max().topython() == [[a.max()]]
    assert dt0.countna().topython() == [[0]]
    assert list_equals(dt0.sum().topython(), [[a.sum()]])
    assert list_equals(dt0.mean().topython(), [[a.mean()]])
    assert list_equals(dt0.sd().topython(), [[a.std(ddof=1)]])


def test_stats_string_large():
    n = 1324578
    a = ["%x" % random.getrandbits(11) for _ in range(n)]
    dt0 = dt.Frame(a)
    assert dt0.countna1() == 0
    assert dt0.nunique1() == n_unique(a)
