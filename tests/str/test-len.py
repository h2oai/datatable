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
from datatable import dt, f, FExpr
from datatable.str import len
from tests import assert_equals



def test_len():
    DT = dt.Frame(A=["", "one", "2", "three", "four", None, "six", "seventy"])
    RES = DT[:, len(f.A)]
    assert RES.stype == dt.stype.int64
    assert RES.to_list() == [[0, 3, 1, 5, 4, None, 3, 7]]


def test_len2():
    DT = dt.Frame([None, "", "mooo" * 10000], stype="str64")
    RES = DT[:, len(f[0])]
    assert RES.stype == dt.stype.int64
    assert RES.to_list() == [[None, 0, 40000]]


def test_len_wrong_col():
    msg = r"Function str.len\(\) cannot be applied to a column of type int32"
    with pytest.raises(TypeError, match=msg):
        DT = dt.Frame(range(34))
        assert DT[:, len(f[0])]


def test_len_unicode():
    DT = dt.Frame(["майдан", "蒙蒂巨蟒", "🤥", "𝔘𝔫𝔦𝔠𝔬𝔡𝔢"])
    RES = DT[:, len(f[0])]
    assert_equals(RES, dt.Frame([6, 4, 1, 7], stype=dt.int64))


def test_len_deprecated():
    DT = dt.Frame(A=["", "one", "2", "three", "four", None, "six", "seventy"])
    with pytest.warns(FutureWarning):
        RES = DT[:, f.A.len()]
    assert RES.stype == dt.stype.int64
    assert RES.to_list() == [[0, 3, 1, 5, 4, None, 3, 7]]
