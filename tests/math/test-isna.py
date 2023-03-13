#!/usr/bin/env python
# -*- coding: utf-8 -*-
#-------------------------------------------------------------------------------
# Copyright 2018-2021 H2O.ai
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
import math
import pytest
from datetime import date
from datatable import dt, f, g, join
from tests import assert_equals


all_sources = [
    [False, True, False, False, True],
    [True, None, None, True, False],
    [5, -3, 6, 3, 0],
    [None, -1, 0, 26, -3],
    [2**31 - 2, -(2**31 - 1), 0, -1, 1],
    [9.5, 0.2, 5.4857301, -3.14159265338979],
    [1.1, 2.3e12, -.5, None, math.inf, 0.0],
    ["foo", "bbar", "baz"],
    [None, "", " ", "  ", None, "\x00"],
    list("qwertyuiiop[]asdfghjkl;'zxcvbnm,./`1234567890-="),
    [date(2001, 1, 4), date(2005, 7, 15), None, None, None]
]




@pytest.mark.parametrize("src", all_sources)
def test_isna(src):
    DT = dt.Frame(src)
    RES = DT[:, dt.math.isna(f[0])]
    assert_equals(RES, dt.Frame([(x is None) for x in src]))


def test_isna2():
    from math import nan
    DT = dt.Frame(A=[1, None, 2, 5, None, 3.6, nan, -4.899])
    DT1 = DT[~dt.math.isna(f.A), :]
    assert DT1.types == DT.types
    assert DT1.names == DT.names
    assert DT1.to_list() == [[1.0, 2.0, 5.0, 3.6, -4.899]]


def test_isna_joined():
    # See issue #2109
    DT = dt.Frame(A=[None, 4, 3, 2, 1])
    JDT = dt.Frame(A=[0, 1, 3, 7],
                   B=['a', 'b', 'c', 'd'],
                   C=[0.25, 0.5, 0.75, 1.0],
                   D=[22, 33, 44, 55],
                   E=[True, False, True, False])
    JDT.key = 'A'
    RES = DT[:, dt.math.isna(g[1:]), join(JDT)]
    dt.internal.frame_integrity_check(RES)
    assert RES.to_list() == [[True, True, False, True, False]] * 4


@pytest.mark.parametrize("src", all_sources[:-1])
def test_isna_scalar(src):
    for val in src:
        assert dt.math.isna(val) == (val is None or val is math.nan)
