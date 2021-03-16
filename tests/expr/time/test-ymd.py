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
from datatable.time import ymd
from datetime import date as d
from tests import assert_equals


def test_ymd_simple():
    DT = dt.Frame(Y=[2001, 2003, 2005, 2020],
                  M=[1, 5, 4, 11],
                  D=[12, 18, 30, 1])
    RES = DT[:, ymd(f.Y, f.M, f.D)]
    assert_equals(RES,
        dt.Frame([d(2001, 1, 12), d(2003, 5, 18), d(2005, 4, 30),
                  d(2020, 11, 1)]))


def test_ymd_named():
    DT = dt.Frame(Y=[2001, 2002, 2003], M=[3, 9, 1], D=[31, 3, 1])
    RES = DT[:, ymd(month=f.M, day=f.D, year=f.Y)]
    assert_equals(RES, dt.Frame([d(2001, 3, 31), d(2002, 9, 3), d(2003, 1, 1)]))


def test_ymd_wrong_types():
    DT = dt.Frame(A=[2000], B=[2.1], C=[1])

    msg = "The year column at index 0 is of type float64, integer expected"
    with pytest.raises(TypeError, match=msg):
        DT[:, ymd(f.B, f.A, f.C)]

    msg = "The month column at index 0 is of type float64, integer expected"
    with pytest.raises(TypeError, match=msg):
        DT[:, ymd(f.A, f.B, f.C)]

    msg = "The day column at index 0 is of type float64, integer expected"
    with pytest.raises(TypeError, match=msg):
        DT[:, ymd(f.A, f.C, f.B)]


def test_ymd_wrong_shape():
    DT = dt.Frame(A=range(5), B=range(5), C=range(5))
    msg = r"Incompatible numbers of columns for the year, month and day " \
          r"arguments of the ymd\(\) function: 3, 2, and 1"
    with pytest.raises(dt.exceptions.InvalidOperationError, match=msg):
        DT[:, ymd(f[:], f[1:], f[0])]


def test_ymd_multi_column():
    DT = dt.Frame(A=range(5), B=range(5), C=range(5))
    RES = DT[:, ymd(2000, 5 + f[:], 1 + 2*f[:])]
    exp = [d(2000, 5 + i, i*2 + 1) for i in range(5)]
    assert_equals(RES, dt.Frame([exp] * 3))


def test_ymd_partial_groupby():
    DT = dt.Frame(A=range(5), B=range(5), C=range(1, 11, 2))
    RES = DT[:, ymd(2000, dt.max(f.B), f.C)]
    assert_equals(RES, dt.Frame([d(2000, 4, 1 + 2*i) for i in range(5)]))


def test_ymd_with_different_types():
    DT = dt.Frame(A=[2010, 2011, 2012]/dt.int32,
                  B=[3, 5, 7]/dt.int8,
                  C=[4, 1, 1]/dt.int64)
    RES = DT[:, ymd(f.A, f.B, f.C)]
    assert_equals(RES, dt.Frame([d(2010, 3, 4), d(2011, 5, 1), d(2012, 7, 1)]))


def test_ymd_nones():
    DT = dt.Frame(A=[1, 2, 3, None, None, 4],
                  B=[1, 2, None, 3, None, 4],
                  C=[1, None, 2, 3, None, 4])
    RES = DT[:, ymd(f.A, f.B, f.C)]
    assert_equals(RES, dt.Frame([d(1, 1, 1), None, None, None, None, d(4, 4, 4)]))


def test_invalid_dates():
    DT = dt.Frame(Y=[2000, 2001, 2002, 2003, 2004],
                  M=[1, 2, 3, 4, 5],
                  D=[-1, 0, 1, 100, 100000])
    assert_equals(DT[:, ymd(f.Y, 2, 29)],
                  dt.Frame([d(2000, 2, 29), None, None, None, d(2004, 2, 29)]))
    assert_equals(DT[:, ymd(2020, f.M, 31)],
                  dt.Frame([d(2020, 1, 31), None, d(2020, 3, 31), None, d(2020, 5, 31)]))
    assert_equals(DT[:, ymd(2020, 1, f.D)],
                  dt.Frame([None, None, d(2020, 1, 1), None, None]))


def test_invalid_months():
    DT = dt.Frame(range(5))
    assert_equals(DT[:, ymd(2021, f[0], 1)],
                  dt.Frame([None] + [d(2021, i, 1) for i in range(1, 5)]))
