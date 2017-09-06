#!/usr/bin/env python3
# Copyright 2017 H2O.ai; Apache License Version 2.0;  -*- encoding: utf-8 -*-
import pytest
import datatable as dt
import statistics
from math import inf, nan
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
            (1.1, 2.3e12, -.5, None, inf, 0.0)}

dt_all = dt_bool | dt_int | dt_float

# Helper function that provides the resulting stype after `min()` or `max()` is
# called
def min_max_stype(stype):
    if stype in ["i1b"]:
        return "i1b"
    if stype in ["i1i", "i2i", "i4i", "i8i"]:
        return "i8i"
    if stype in ["f4r", "f8r"]:
        return "f8r"
    return stype

# Helper function that provides the resulting stype after `mean()` or `sd()` is
# called
def mean_sd_stype(stype):
    if stype in ["i1i", "i2i", "i4i", "i8i", "f4r", "f8r"]:
        return "f8r"
    return stype


#-------------------------------------------------------------------------------
# Minimum function dt.min()
#-------------------------------------------------------------------------------

def t_min(t):
    t = [i for i in t if i is not None]
    if len(t) == 0:
        return [None]
    else:
        return [min(t)]


@pytest.mark.parametrize("src", dt_all)
def test_dt_min(src):
    dt0 = dt.DataTable(src)
    dtr = dt0.min()
    assert dtr.internal.check()
    assert list(dtr.stypes) == [min_max_stype(s) for s in dt0.stypes]
    assert dtr.shape == (1, dt0.shape[1])
    for i in range(dt0.ncols):
        assert dt0.names[i] == dtr.names[i]
    assert dtr.topython() == [t_min(src)]


#-------------------------------------------------------------------------------
# Maximum function dt.max()
#-------------------------------------------------------------------------------

def t_max(t):
    t = [i for i in t if i is not None]
    if len(t) == 0:
        return [None]
    else:
        return [max(t)]


@pytest.mark.parametrize("src", dt_all)
def test_dt_max(src):
    dt0 = dt.DataTable(src)
    dtr = dt0.max()
    assert dtr.internal.check()
    assert list(dtr.stypes) == [min_max_stype(s) for s in dt0.stypes]
    assert dtr.shape == (1, dt0.shape[1])
    for i in range(dt0.ncols):
        assert dt0.names[i] == dtr.names[i]
    assert dtr.topython() == [t_max(src)]



#-------------------------------------------------------------------------------
# Mean function dt.mean()
#-------------------------------------------------------------------------------

def t_mean(t):
    t = [i for i in t if i is not None and not isinstance(i, bool)]
    if len(t) == 0:
        return [None]
    else:
        return [statistics.mean(t)]

@pytest.mark.parametrize("src", dt_all)
def test_dt_mean(src):
    dt0 = dt.DataTable(src)
    dtr = dt0.mean()
    assert dtr.internal.check()
    assert list(dtr.stypes) == [mean_sd_stype(s) for s in dt0.stypes]
    assert dtr.shape == (1, dt0.shape[1])
    assert dt0.names == dtr.names
    assert list_equals(dtr.topython(), [t_mean(src)])


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
                                      ([-inf, 0, inf], nan),
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
    t = [i for i in t if i is not None and not isinstance(i, bool)]
    if len(t) == 0:
        return [None]
    elif len(t) == 1:
        return [0.0]
    else:
        return [statistics.stdev(t)]

@pytest.mark.parametrize("src", dt_all)
def test_dt_sd(src):
    dt0 = dt.DataTable(src)
    dtr = dt0.sd()
    assert dtr.internal.check()
    assert list(dtr.stypes) == [mean_sd_stype(s) for s in dt0.stypes]
    assert dtr.shape == (1, dt0.shape[1])
    assert dt0.names == dtr.names
    assert list_equals(dtr.topython(), [t_sd(src)])


@pytest.mark.parametrize("src, res", [([1, 3, 5, None], 2.0),
                                      ([], None),
                                      ([3.0], 0),
                                      ([None, None, None], None),
                                      ([inf, inf, inf], nan),
                                      ([-inf, 0, inf], nan),
                                      ])
def test_dt_sd_special_cases(src, res):
    dt0 = dt.DataTable(src)
    dtr = dt0.sd()
    assert dtr.internal.check()
    assert list_equals(dtr.topython(), [[res]])
