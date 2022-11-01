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
from datatable import dt, f, cumsum, FExpr, by
from tests import assert_equals


#-------------------------------------------------------------------------------
# Errors
#-------------------------------------------------------------------------------

def test_cumsum_non_numeric():
    DT = dt.Frame(list('abcde'))
    with pytest.raises(TypeError, match = r'Invalid column of type str32 in cumsum'):
        DT[:, cumsum(f[0])]

def test_cumsum_non_numeric_by():
    DT = dt.Frame(list('abcde'))
    with pytest.raises(TypeError,  match = r'Invalid column of type str32 in cumsum'):
        DT[:, cumsum(f[0]), by(f[0])]

def test_cumsum_no_argument():
    match = r'Function datatable.cumsum\(\) requires at least 1 positional argument, ' \
             'but none were given'
    with pytest.raises(TypeError, match = match):
        dt.cumsum()


#-------------------------------------------------------------------------------
# Normal
#-------------------------------------------------------------------------------

def test_cumsum_str():
  assert str(cumsum(f.A)) == "FExpr<cumsum(f.A, reverse=False)>"
  assert str(cumsum(f.A, True)) == "FExpr<cumsum(f.A, reverse=True)>"
  assert str(cumsum(f.A) + 1) == "FExpr<cumsum(f.A, reverse=False) + 1>"
  assert str(cumsum(f.A + f.B)) == "FExpr<cumsum(f.A + f.B, reverse=False)>"
  assert str(cumsum(f.B)) == "FExpr<cumsum(f.B, reverse=False)>"
  assert str(cumsum(f[:2])) == "FExpr<cumsum(f[:2], reverse=False)>"
  assert str(cumsum(f[:2], reverse=True)) == "FExpr<cumsum(f[:2], reverse=True)>"


def test_cumsum_empty_frame():
    DT = dt.Frame()
    expr_cumsum = cumsum(DT)
    assert isinstance(expr_cumsum, FExpr)
    assert_equals(DT[:, cumsum(f[:])], DT)


def test_cumsum_void():
    DT = dt.Frame([None]*10)
    DT_cumsum = DT[:, cumsum(f[:])]
    assert_equals(DT_cumsum, dt.Frame([0]*10/dt.int64))


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

def test_cumsum_reverse():
    DT = dt.Frame([range(5), [-1, 1, None, 2, 5.5]])
    DT_cumsum = DT[:, cumsum(f[:], reverse=True)]
    DT_ref = DT[::-1, cumsum(f[:])][::-1, :]
    assert_equals(DT_cumsum, DT_ref)


def test_cumsum_groupby():
    DT = dt.Frame([[2, 1, 1, 1, 2], [1.5, -1.5, math.inf, 2, 3]])
    DT_cumsum = DT[:, cumsum(f[:]), by(f[0])]
    DT_ref = dt.Frame([[1, 1, 1, 2, 2], [-1.5, math.inf, math.inf, 1.5, 4.5]/dt.float64])
    assert_equals(DT_cumsum, DT_ref)

def test_cumsum_groupby_reverse():
    DT = dt.Frame([[2, 1, 1, 1, 2], [1.5, -1.5, math.inf, 2, 3]])
    DT_cumsum = DT[:, cumsum(f[:], reverse=True), by(f[0])]
    DT_ref = dt.Frame([[1, 1, 1, 2, 2], [math.inf, math.inf, 2.0, 4.5, 3.0]/dt.float64])
    assert_equals(DT_cumsum, DT_ref)


def test_cumsum_void_grouped_column():
    DT = dt.Frame([None]*10)
    DT_cumsum = DT[:, cumsum(f.C0), by(f.C0)]
    assert_equals(DT_cumsum, dt.Frame([[None]*10, [0]*10/dt.int64]))


def test_cumsum_grouped_column():
    DT = dt.Frame([2, 1, None, 1, 2])
    DT_cumsum = DT[:, cumsum(f[0]), by(f[0])]
    DT_ref = dt.Frame([[None, 1, 1, 2, 2], [0, 1, 2, 2, 4]/dt.int64])
    assert_equals(DT_cumsum, DT_ref)


def test_cumsum_groupby_complex():
    DT = dt.Frame([[3, 14, 15, 92, 6], ["a", "cat", "a", "dog", "cat"]])
    DT_cumsum = DT[:, cumsum(f[0].min()), by(f[1])]

    DT_ref = dt.Frame({
               "C1" : ["a", "a", "cat", "cat", "dog"],
               "C0" : [3, 6, 6, 12, 92]/dt.int64
             })
    assert_equals(DT_cumsum, DT_ref)
