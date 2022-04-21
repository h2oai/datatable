#!/usr/bin/env python
# -*- coding: utf-8 -*-
#-------------------------------------------------------------------------------
# Copyright 2022 H2O.ai
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
import random
from datatable import dt, stype, f, cumsum, FExpr
from tests import assert_equals


#-------------------------------------------------------------------------------
# Normal
#-------------------------------------------------------------------------------

def test_cumsum_empty_frame():
    DT = dt.Frame()
    expr_cumsum = cumsum(DT)
    assert isinstance(expr_cumsum, FExpr)
    assert_equals(DT[:, f[:]], DT)


def test_cumsum_void():
    DT = dt.Frame([None, None, None])
    DT_cumsum = DT[:, cumsum(f[:])]
    assert_equals(DT_cumsum, dt.Frame([0, 0, 0]/dt.int64))


def test_cumsum_trivial():
    DT = dt.Frame([0]/dt.int64)
    cumsum_fexpr = cumsum(f[:])
    DT_cumsum = DT[:, cumsum_fexpr]
    assert isinstance(cumsum_fexpr, FExpr)
    assert_equals(DT, DT_cumsum)


def test_cumsum_small():
    DT = dt.Frame([range(5), [-1, 1, None, 2, 5.5]])
    DT_cumsum = DT[:, cumsum(f[:])]
    DT_ref = dt.Frame([[0, 1, 3, 6, 10]/dt.int64, [-1, 0, 0, 2, 7.5]])
    assert_equals(DT_cumsum, DT_ref)


