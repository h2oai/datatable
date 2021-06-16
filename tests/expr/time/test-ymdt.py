#!/usr/bin/env python
# -*- coding: utf-8 -*-
#-------------------------------------------------------------------------------
# Copyright 2021 H2O.ai
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
import pytest
from datatable import dt, f
from datatable.time import ymdt
from datetime import date as d
from datetime import datetime as t
from tests import assert_equals


def test_ymdt_unnamed():
    DT = dt.Frame(Y=[2001, 2003, 2005, 2020, 1960],
                  M=[1, 5, 4, 11, 8],
                  D=[12, 18, 30, 1, 14],
                  h=[7, 14, 22, 23, 12],
                  m=[15, 30, 0, 59, 0],
                  s=[12, 23, 0, 59, 27],
                  ns=[0, 0, 0, 999999000, 123000])
    RES = DT[:, ymdt(f.Y, f.M, f.D, f.h, f.m, f.s, f.ns)]
    assert_equals(RES,
        dt.Frame([t(2001, 1, 12, 7, 15, 12),
                  t(2003, 5, 18, 14, 30, 23),
                  t(2005, 4, 30, 22, 0, 0),
                  t(2020, 11, 1, 23, 59, 59, 999999),
                  t(1960, 8, 14, 12, 0, 27, 123)]))


def test_ymdt_named():
    DT = dt.Frame(Y=[2021, 1965, 2003, 1901, 1999],
                  M=[10, 7, 4, 11, 8],
                  D=[19, 18, 29, 10, 21],
                  h=[7, 21, 22, 23, 19],
                  m=[37, 29, 0, 44, 0],
                  s=[19, 23, 0, 46, 27],
                  ns=[0, 0, 0, 123456000, 1000])
    RES = DT[:, ymdt(year=f.Y, month=f.M, day=f.D, hour=f.h, minute=f.m,
                     second=f.s, nanosecond=f.ns)]
    assert_equals(RES,
        dt.Frame([t(2021, 10, 19, 7, 37, 19),
                  t(1965, 7, 18, 21, 29, 23),
                  t(2003, 4, 29, 22, 0, 0),
                  t(1901, 11, 10, 23, 44, 46, 123456),
                  t(1999, 8, 21, 19, 0, 27, 1)]))


def test_ymdt_ns_optional():
    DT = dt.Frame([[2001], [12], [31], [23], [59], [59]])
    RES = DT[:, ymdt(f[0], f[1], f[2], f[3], f[4], f[5])]
    assert_equals(RES, dt.Frame([t(2001, 12, 31, 23, 59, 59)]))


def test_ymdt_with_date():
    DT = dt.Frame(date=[d(2001, 3, 15), d(2017, 11, 23), d(1930, 5, 14), None],
                  hour=[12, 19, 2, 7], min=[30, 40, 50, 20])
    RES = DT[:, ymdt(date=f.date, hour=f.hour, minute=f.min, second=f.min//2)]
    assert_equals(RES, dt.Frame([t(2001, 3, 15, 12, 30, 15),
                                 t(2017, 11, 23, 19, 40, 20),
                                 t(1930, 5, 14, 2, 50, 25),
                                 None]))


@pytest.mark.parametrize('arg', ['year', 'month', 'day', 'hour', 'minute',
                                 'second', 'nanosecond'])
def test_ymdt_wrong_types(arg):
    DT = dt.Frame(A=[2000], B=[2.1], C=[11], D=[4])
    kwds = dict(year=f.A, month=f.C, day=f.D, hour=f.D, minute=f.C, second=0)
    kwds[arg] = f.B
    msg = "The %s column is not integer" % arg
    with pytest.raises(TypeError, match=msg):
        DT[:, ymdt(**kwds)]


def test_ymdt_wrong_shape():
    DT = dt.Frame(A=range(5), B=range(5), C=range(5))
    msg = r"Incompatible number of columns for the arguments of " \
          r"time.ymdt\(\) function"
    with pytest.raises(dt.exceptions.InvalidOperationError, match=msg):
        DT[:, ymdt(f[:], f[1:], f[0], 0, 0, 0)]


def test_ymdt_wrong_shape2():
    DT = dt.Frame(A=range(5), B=range(5), C=range(5))
    msg = r"Incompatible number of columns for the arguments of " \
          r"time.ymdt\(\) function"
    with pytest.raises(dt.exceptions.InvalidOperationError, match=msg):
        DT[:, ymdt(f.A, f.B, f.C, f[:2], 0, 1, f[:])]


def test_ymd_and_date():
    DT = dt.Frame(A=[d(2000, 1, 1)], y=[2000], m=[1], d=[1])
    msg = "When argument date= is provided, arguments year=, month= and " \
          "day= cannot be used."
    with pytest.raises(TypeError, match=msg):
        DT[:, ymdt(date=f.A, year=f.y, month=f.m, day=f.d, hour=0,
                   minute=0, second=0)]


def test_ymdt_multi_column():
    DT = dt.Frame(A=range(5), B=range(5), C=range(5))
    RES = DT[:, ymdt(2000, 5 + f[:], 1 + 2*f[:], 0, 10 * f[:], 30)]
    exp = [t(2000, 5 + i, i*2 + 1, 0, 10*i, 30) for i in range(5)]
    assert_equals(RES, dt.Frame([exp] * 3))


def test_ymdt_partial_groupby():
    DT = dt.Frame(A=range(5), B=range(5), C=range(1, 11, 2))
    RES = DT[:, ymdt(2000, dt.max(f.B), f.C, 0, 0, 0)]
    assert_equals(RES, dt.Frame([t(2000, 4, 1 + 2*i, 0, 0, 0) for i in range(5)]))


def test_ymdt_with_different_types():
    DT = dt.Frame(A=[2010, 2011, 2012]/dt.int32,
                  B=[3, 5, 7]/dt.int8,
                  C=[4, 1, 1]/dt.int64,
                  D=[12, 10, 3]/dt.int16,
                  E=[30, 50, 25]/dt.int32,
                  F=[0, 0, 0]/dt.int8)
    RES = DT[:, ymdt(f.A, f.B, f.C, f.D, f.E, f.F)]
    assert_equals(RES, dt.Frame([t(2010, 3, 4, 12, 30, 0),
                                 t(2011, 5, 1, 10, 50, 0),
                                 t(2012, 7, 1, 3, 25, 0)]))


def test_ymdt_nones():
    DT = dt.Frame(A=[1, 2, 3, None, None, 4],
                  B=[1, 2, None, 3, None, 4],
                  C=[1, None, 2, 3, None, 4])
    RES = DT[:, ymdt(2001, f.B, f.C, f.A, f.B, f.C)]
    assert_equals(RES, dt.Frame([t(2001, 1, 1, 1, 1, 1),
                                 None, None, None, None,
                                 t(2001, 4, 4, 4, 4, 4)]))


def test_invalid_dates():
    DT = dt.Frame(Y=[2000, 2001, 2002, 2003, 2004],
                  M=[1, 2, 3, 4, 5],
                  D=[-1, 0, 1, 100, 100000])
    assert_equals(DT[:, ymdt(f.Y, 2, 29, 0, 0, 0)],
                  dt.Frame([t(2000, 2, 29, 0, 0, 0), None, None, None,
                            t(2004, 2, 29, 0, 0, 0)]))
    assert_equals(DT[:, ymdt(2020, f.M, 31, 0, 0, 0)],
                  dt.Frame([t(2020, 1, 31, 0, 0, 0), None,
                            t(2020, 3, 31, 0, 0, 0), None,
                            t(2020, 5, 31, 0, 0, 0)]))
    assert_equals(DT[:, ymdt(2020, 1, f.D, 0, 0, 0)],
                  dt.Frame([None, None, t(2020, 1, 1, 0, 0, 0), None, None]))


def test_invalid_months():
    DT = dt.Frame(range(5))
    assert_equals(DT[:, ymdt(2021, f[0], 1, 0, 0, 0)],
                  dt.Frame([None] + [t(2021, i, 1, 0, 0, 0)
                                     for i in range(1, 5)]))


def test_large_time():
    DT = dt.Frame(date=[d(2001, 1, 1)] * 7, hours=range(-60, 61, 20))
    RES = DT[:, ymdt(date=f.date, hour=f.hours, minute=0, second=0)]
    assert_equals(RES, dt.Frame([t(2000, 12, 29, 12, 0, 0),
                                 t(2000, 12, 30, 8, 0, 0),
                                 t(2000, 12, 31, 4, 0, 0),
                                 t(2001, 1, 1, 0, 0, 0),
                                 t(2001, 1, 1, 20, 0, 0),
                                 t(2001, 1, 2, 16, 0, 0),
                                 t(2001, 1, 3, 12, 0, 0)]))
