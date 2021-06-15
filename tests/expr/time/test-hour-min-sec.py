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
from datatable.time import hour, minute, second, nanosecond
from datetime import datetime as t
from tests import assert_equals


def test_hour_minute_second_normal():
    DT = dt.Frame(T=[t(2001, 5, 17, 11, 24, 50),
                     t(2011, 11, 30, 17, 1, 29, 7623),
                     None,
                     t(2021, 8, 19, 5, 55, 34, 98078),
                     t(1960, 2, 11, 14, 21, 7, 123456)])
    RES = DT[:, {"hour": hour(f.T),
                 "minute": minute(f.T),
                 "second": second(f.T),
                 "nanosecond": nanosecond(f.T)}]
    assert_equals(RES,
        dt.Frame(hour=[11, 17, None, 5, 14],
                 minute=[24, 1, None, 55, 21],
                 second=[50, 29, None, 34, 7],
                 nanosecond=[0, 7623000, None, 98078000, 123456000]))


def test_hour_minute_second_small():
    DT = dt.Frame([0, 1, -1], stype='int64')
    DT[0] = dt.Type.time64
    RES = DT[:, {"hour": hour(f[0]),
                 "minute": minute(f[0]),
                 "second": second(f[0]),
                 "nanosecond": nanosecond(f[0])}]
    assert_equals(RES,
        dt.Frame(hour=[0, 0, 23],
                 minute=[0, 0, 59],
                 second=[0, 0, 59],
                 nanosecond=[0, 1, 999999999]))


def test_noargs():
    DT = dt.Frame(range(5))
    DT[0] = dt.Type.time64

    with pytest.raises(TypeError):
        DT[:, hour()]
    with pytest.raises(TypeError):
        DT[:, minute()]
    with pytest.raises(TypeError):
        DT[:, second()]
    with pytest.raises(TypeError):
        DT[:, nanosecond()]


def test_invalid_type():
    DT = dt.Frame([1, 3, 5, 9], stype='date32')

    msg = r"Function time.hour\(\) requires a time64 column"
    with pytest.raises(TypeError, match=msg):
        DT[:, hour(f[0])]

    msg = r"Function time.minute\(\) requires a time64 column"
    with pytest.raises(TypeError, match=msg):
        DT[:, minute(f[0])]

    msg = r"Function time.second\(\) requires a time64 column"
    with pytest.raises(TypeError, match=msg):
        DT[:, second(f[0])]

    msg = r"Function time.nanosecond\(\) requires a time64 column"
    with pytest.raises(TypeError, match=msg):
        DT[:, nanosecond(f[0])]



def test_void_column():
    DT = dt.Frame([None] * 5)
    RES = DT[:, [hour(f[0]), minute(f[0]), second(f[0]), nanosecond(f[0])]]
    assert_equals(RES, dt.Frame([[None] * 5] * 4))
