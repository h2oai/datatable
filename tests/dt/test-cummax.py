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
from datatable import dt, f, cummax, FExpr, by
from tests import assert_equals


#-------------------------------------------------------------------------------
# Errors
#-------------------------------------------------------------------------------

def test_cummax_non_numeric():
    DT = dt.Frame(list('abcde'))
    with pytest.raises(TypeError, match = r'Invalid column of type str32 in cummax'):
        DT[:, cummax(f[0])]

def test_cummax_non_numeric_by():
    DT = dt.Frame(list('abcde'))
    with pytest.raises(TypeError,  match = r'Invalid column of type str32 in cummax'):
        DT[:, cummax(f[0]), by(f[0])]

def test_cummax_no_argument():
    match = r'Function datatable.cummax\(\) requires exactly 1 positional argument, ' \
             'but none were given'
    with pytest.raises(TypeError, match = match):
        dt.cummax()


#-------------------------------------------------------------------------------
# Normal
#-------------------------------------------------------------------------------

def test_cummax_str():
  assert str(cummax(f.A)) == "FExpr<cummax(f.A)>"
  assert str(cummax(f.A) + 1) == "FExpr<cummax(f.A) + 1>"
  assert str(cummax(f.A + f.B)) == "FExpr<cummax(f.A + f.B)>"
  assert str(cummax(f.B)) == "FExpr<cummax(f.B)>"
  assert str(cummax(f[:2])) == "FExpr<cummax(f[:2])>"


def test_cummax_empty_frame():
    DT = dt.Frame()
    expr_cummax = cummax(DT)
    assert isinstance(expr_cummax, FExpr)
    assert_equals(DT[:, f[:]], DT)


def test_cummax_void():
    DT = dt.Frame([None, None, None])
    DT_cummax = DT[:, cummax(f[:])]
    assert_equals(DT_cummax, DT)


def test_cummax_trivial():
    DT = dt.Frame([0]/dt.int64)
    cummax_fexpr = cummax(f[:])
    DT_cummax = DT[:, cummax_fexpr]
    assert isinstance(cummax_fexpr, FExpr)
    assert_equals(DT, DT_cummax)


def test_cummax_bool():
    DT = dt.Frame([None, False, None, True, False, True])
    DT_cummax = DT[:, cummax(f[:])]
    assert_equals(DT_cummax, dt.Frame([None, False, False, True, True, True]))


def test_cummax_small():
    DT = dt.Frame([range(5), [None, -1, None, 5.5, 3]])
    DT_cummax = DT[:, cummax(f[:])]
    DT_ref = dt.Frame([[0, 1, 2, 3, 4], [None, -1, -1, 5.5, 5.5]])
    assert_equals(DT_cummax, DT_ref)


def test_cummax_groupby():
    DT = dt.Frame([[2, 1, 1, 1, 2], [1.5, -1.5, math.inf, None, 3]])
    DT_cummax = DT[:, cummax(f[:]), by(f[0])]
    DT_ref = dt.Frame([[1, 1, 1, 2, 2], [-1.5, math.inf, math.inf, 1.5, 3]/dt.float64])
    assert_equals(DT_cummax, DT_ref)


def test_cummax_grouped_column():
    DT = dt.Frame([2, 1, None, 1, 2])
    DT_cummax = DT[:, cummax(f[0]), by(f[0])]
    DT_ref = dt.Frame([[None, 1, 1, 2, 2], [None, 1, 1, 2, 2]])
    assert_equals(DT_cummax, DT_ref)


