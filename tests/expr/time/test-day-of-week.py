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
import random
from datatable import dt, f
from datatable.time import day_of_week
from datetime import date as d
from datetime import datetime as t
from tests import assert_equals


def test_day_of_week_with_date32():
    DT = dt.Frame([d(2021, 3, 15), d(2021, 3, 16), d(2021, 3, 17),
                   d(2021, 1, 1), d(2020, 2, 28), d(2020, 2, 29), None])
    RES = DT[:, day_of_week(f[0])]
    assert_equals(RES, dt.Frame([1, 2, 3, 5, 5, 6, None]))


def test_day_of_week_with_time64():
    DT = dt.Frame([t(2001, 3, 15, 0, 0, 0),
                   t(2017, 11, 1, 23, 59, 59, 999999),
                   None,
                   t(1970, 1, 1, 0, 0, 0, 1),
                   t(1969, 12, 31, 23, 59, 59, 999999),
                   t(1900, 12, 13, 0, 0, 0)])
    RES = DT[:, day_of_week(f[:])]
    assert_equals(RES, dt.Frame([4, 3, None, 4, 3, 4]))


def test_noarg():
    msg = r"Function .*day_of_week\(\) requires exactly 1 positional argument"
    with pytest.raises(TypeError, match=msg):
        day_of_week()


def test_void_column():
    DT = dt.Frame([None] * 10)
    assert DT.types == [dt.Type.void]
    RES = DT[:, day_of_week(f[0])]
    assert_equals(RES, DT)


def test_wrong_type():
    DT = dt.Frame(A=[1, 4, 10], B=[7.4, 0.0, -1],
                  C=['2000-01-01', None, '2001-02-02'])
    msg = r"Function time\.day_of_week\(\) requires a date32 or time64 column"
    for i in range(3):
        with pytest.raises(TypeError, match=msg):
            DT[:, day_of_week(f[i])]


def test_negative_days():
    DT = dt.Frame([0, -1, -2, -3, -4, -5, -100, -1000, -100000], stype='date32')
    RES = DT[:, day_of_week(f[0])]
    assert_equals(RES, dt.Frame([
        4,  # 1970-01-01: Thu
        3,  # 1969-12-31: Wed
        2,  # 1969-12-30: Tue
        1,  # 1969-12-29: Mon
        7,  # 1969-12-28: Sun
        6,  # 1969-12-27: Sat
        2,  # 1969-09-23: Tue
        5,  # 1967-04-07: Fri
        6,  # 1696-03-17: Sat
    ]))


@pytest.mark.parametrize("seed", [random.getrandbits(32)])
def test_day_of_week_random(seed):
    # Check that the result is the same as python's .isoweekday()
    random.seed(seed)
    n = int(random.expovariate(0.01) + 11)
    src = [int(random.random() * 10000) for i in range(n)]
    DT = dt.Frame(src, stype='date32')
    RES = DT[:, day_of_week(f[0])]
    assert RES.to_list()[0] == [x.isoweekday() for x in DT.to_list()[0]]
