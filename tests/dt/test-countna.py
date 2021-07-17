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
from datatable import dt, f
from datatable.internal import frame_integrity_check
from tests import assert_equals
from math import inf, nan


srcs_bool = [[False, True, False, False, True],
             [True, None, None, True, False],
             [True], [False], [None] * 10]
srcs_int = [[5, -3, 6, 3, 0],
            [None, -1, 0, 26, -3],
            [129, 38, 27, -127, 8],
            [385, None, None, -3, -89],
            [-192, 32769, 683, 94, 0],
            [None, -32788, -4, -44444, 5],
            [30, -284928, 59, 3, 2147483649],
            [2147483648, None, None, None, None],
            [-1, 1], [100], [0]]
srcs_real = [[9.5, 0.2, 5.4857301, -3.14159265358979],
             [1.1, 2.3e12, -.5, None, inf, 0.0],
             [3.5, 2.36, nan, 696.9, 4097],
             [3.1415926535897932], [nan]]

srcs_str = [["foo", None, "bar", "baaz", None],
            ["a", "c", "d", None, "d", None, None, "a", "e", "c", "a", "a"],
            ["leeeeeroy!"],
            ["abc", None, "def", "abc", "a", None, "a", "ab"] / dt.str64,
            [None, "integrity", None, None, None, None, None] / dt.str64,
            ["f", "c", "e", "a", "c", "d", "f", "c", "e", "A", "a"] / dt.str64]

srcs_numeric = srcs_bool + srcs_int + srcs_real
srcs_all = srcs_numeric + srcs_str

#-------------------------------------------------------------------------------
#  countna
#-------------------------------------------------------------------------------


@pytest.mark.parametrize("src", srcs_all)
def test_dt_count_na1(src):
    df = dt.Frame(src)
    EXP = df.countna()
    RES = df[:, dt.countna(f[:])]
    assert_equals(EXP, RES)


def test_dt_count_na2():
    DT = dt.Frame(G=[1,1,1,2,2,2], V=[None, None, None, None, 3, 5])
    EXP = dt.Frame(G=[1,2], V1=[3,1], V2=[3,0])
    RES = DT[:, [dt.countna(f.V), dt.countna(dt.mean(f.V))], dt.by(f.G)]
    assert EXP.to_list() == RES.to_list()
