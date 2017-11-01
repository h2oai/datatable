#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-
import pytest
import datatable as dt
import statistics
from datatable import stype
from math import inf, nan, isnan
from tests import list_equals


# Sets of tuples containing test columns of each type
dt_bool = {(False, True, False, False, True),
           (True, None, None, True, False)}

dt_int = {(5, -3, 6, 3, 0),
          (None, -1, 0, 26, -3),
          (129, 38, 27, -127, 8),
          (385, None, None, -3, -89),
          (-192, 32769, 683, 94, 0),
          (None, -32788, -4, -44444, 5),
          (30, -284928, 59, 3, 2147483649),
          (2147483648, None, None, None, None)
          }

dt_float = {(9.5, 0.2, 5.4857301, -3.14159265358979),
            (1.1, 2.3e12, -.5, None, inf, 0.0),
            (3.5, 2.36, nan, 696.9, 4097)}

dt_all = dt_bool | dt_int | dt_float

@pytest.fixture(params=dt_all)
def src_all(request):
    src = request.param
    return src

# Helper function that provides the resulting stype after `min()` or `max()` is
# called
def min_max_stype(st):
    return st

# Helper function that provides the resulting stype after `mean()` or `sd()` is
# called
def mean_sd_stype(st):
    return stype.float64

# Helper function that provides the result stype after `sum()` is called
def sum_stype(st):
    if st in [stype.bool8, stype.int8, stype.int16, stype.int32, stype.int64]:
        return stype.int64
    if st in [stype.float32, stype.float64]:
        return stype.float64
    return st


#-------------------------------------------------------------------------------
# Minimum function dt.min()
#-------------------------------------------------------------------------------

def t_min(t):
    t = [i for i in t if i is not None and not isnan(i)]
    if len(t) == 0:
        return [None]
    else:
        return [min(t)]


def test_dt_min(src_all):
    dt0 = dt.DataTable(src_all)
    dtr = dt0.min()
    assert dtr.internal.check()
    assert list(dtr.stypes) == [min_max_stype(s) for s in dt0.stypes]
    assert dtr.shape == (1, dt0.ncols)
    for i in range(dt0.ncols):
        assert dt0.names[i] == dtr.names[i]
    assert dtr.topython() == [t_min(src_all)]


def test_dt_str():
    dt0 = dt.DataTable([[1, 5, 3, 9, -2], list("abcde")])
    dtr = dt0.min()
    assert dtr.internal.check()
    assert dtr.topython() == [[-2], [None]]



#-------------------------------------------------------------------------------
# Maximum function dt.max()
#-------------------------------------------------------------------------------

def t_max(t):
    t = [i for i in t if i is not None and not isnan(i)]
    if len(t) == 0:
        return [None]
    else:
        return [max(t)]


def test_dt_max(src_all):
    dt0 = dt.DataTable(src_all)
    dtr = dt0.max()
    assert dtr.internal.check()
    assert list(dtr.stypes) == [min_max_stype(s) for s in dt0.stypes]
    assert dtr.shape == (1, dt0.ncols)
    for i in range(dt0.ncols):
        assert dt0.names[i] == dtr.names[i]
    assert dtr.topython() == [t_max(src_all)]


#-------------------------------------------------------------------------------
# Sum function dt.sum()
#-------------------------------------------------------------------------------

def t_sum(t):
    t = [i for i in t if i is not None and not isnan(i)]
    if len(t) == 0:
        return [0]
    else:
        return [sum(t)]

def test_dt_sum(src_all):
    dt0 = dt.DataTable(src_all)
    dtr = dt0.sum()
    assert dtr.internal.check()
    assert list(dtr.stypes) == [sum_stype(s) for s in dt0.stypes]
    assert dtr.shape == (1, dt0.ncols)
    assert dt0.names == dtr.names
    assert list_equals(dtr.topython(), [t_sum(src_all)])


#-------------------------------------------------------------------------------
# Mean function dt.mean()
#-------------------------------------------------------------------------------

def t_mean(t):
    t = [i for i in t
         if i is not None and not isnan(i)]
    if len(t) == 0:
        return [None]
    else:
        return [statistics.mean(t)]

def test_dt_mean(src_all):
    dt0 = dt.DataTable(src_all)
    dtr = dt0.mean()
    assert dtr.internal.check()
    assert list(dtr.stypes) == [mean_sd_stype(s) for s in dt0.stypes]
    assert dtr.shape == (1, dt0.ncols)
    assert dt0.names == dtr.names
    assert list_equals(dtr.topython(), [t_mean(src_all)])


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
    dt0 = dt.DataTable(src)
    dtr = dt0.mean()
    assert dtr.internal.check()
    assert list_equals(dtr.topython(), [[res]])



#-------------------------------------------------------------------------------
# Standard Deviation function dt.sd()
#-------------------------------------------------------------------------------

def t_sd(t):
    t = [i for i in t
         if i is not None and not isnan(i)]
    if len(t) == 0:
        return [None]
    elif len(t) == 1:
        return [0.0]
    else:
        res = statistics.stdev(t)
        return [res if not isnan(res) else None]

def test_dt_sd(src_all):
    dt0 = dt.DataTable(src_all)
    dtr = dt0.sd()
    assert dtr.internal.check()
    assert list(dtr.stypes) == [mean_sd_stype(s) for s in dt0.stypes]
    assert dtr.shape == (1, dt0.ncols)
    assert dt0.names == dtr.names
    assert list_equals(dtr.topython(), [t_sd(src_all)])


@pytest.mark.parametrize("src, res", [([1, 3, 5, None], 2.0),
                                      ([], None),
                                      ([3.0], 0),
                                      ([None, None, None], None),
                                      ([inf, inf, inf], None),
                                      ([-inf, 0, inf], None),
                                      ])
def test_dt_sd_special_cases(src, res):
    dt0 = dt.DataTable(src)
    dtr = dt0.sd()
    assert dtr.internal.check()
    assert list_equals(dtr.topython(), [[res]])


#-------------------------------------------------------------------------------
# Count_na function dt.count_na()
#-------------------------------------------------------------------------------

def t_count_na(t):
    return [len([i for i in t if i is None or isnan(i)])]

def test_dt_count_na(src_all):
    dt0 = dt.DataTable(src_all)
    dtr = dt0.countna()
    assert dtr.internal.check()
    assert list(dtr.stypes) == [stype.int64] * dt0.ncols
    assert dtr.shape == (1, dt0.ncols)
    assert dt0.names == dtr.names
    assert list_equals(dtr.topython(), [t_count_na(src_all)])


#-------------------------------------------------------------------------------
# RowIndex compatability
#-------------------------------------------------------------------------------
ridx_param = [([-3, 6, 1, 0, 4], slice(2, 5), 0),
              ([-3, 6, 1, 0, 4], [1, 4], 4),
              ([3.5, -182, None, 2, 3], slice(2, None, 2), 3),
              ([3.5, -182, None, 2, 3], [0, 2, 4], 3),
              ([True, True, True, None, False], slice(4), True),
              ([True, True, True, None, False], [0, 3], True)]

@pytest.mark.parametrize("src,view,exp", ridx_param)
def test_dt_ridx(src, view, exp):
    dt0 = dt.DataTable(src)
    dt_view = dt0(view)
    res = dt_view.min()
    assert res.topython()[0][0] == exp
