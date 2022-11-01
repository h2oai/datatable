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
from datatable import dt, f, cumprod, FExpr, by
from tests import assert_equals


#-------------------------------------------------------------------------------
# Errors
#-------------------------------------------------------------------------------

def test_cumprod_non_numeric():
    DT = dt.Frame(list('abcde'))
    with pytest.raises(TypeError, match = r'Invalid column of type str32 in cumprod'):
        DT[:, cumprod(f[0])]

def test_cumprod_non_numeric_by():
    DT = dt.Frame(list('abcde'))
    with pytest.raises(TypeError,  match = r'Invalid column of type str32 in cumprod'):
        DT[:, cumprod(f[0]), by(f[0])]

def test_cumprod_no_argument():
    match = r'Function datatable.cumprod\(\) requires at least 1 positional argument, ' \
             'but none were given'
    with pytest.raises(TypeError, match = match):
        dt.cumprod()


#-------------------------------------------------------------------------------
# Normal
#-------------------------------------------------------------------------------

def test_cumprod_str():
  assert str(cumprod(f.A)) == "FExpr<cumprod(f.A, reverse=False)>"
  assert str(cumprod(f.A, True)) == "FExpr<cumprod(f.A, reverse=True)>"
  assert str(cumprod(f.A) + 1) == "FExpr<cumprod(f.A, reverse=False) + 1>"
  assert str(cumprod(f.A + f.B)) == "FExpr<cumprod(f.A + f.B, reverse=False)>"
  assert str(cumprod(f.A + f.B, reverse=True)) == "FExpr<cumprod(f.A + f.B, reverse=True)>"
  assert str(cumprod(f.B)) == "FExpr<cumprod(f.B, reverse=False)>"
  assert str(cumprod(f[:2])) == "FExpr<cumprod(f[:2], reverse=False)>"
  assert str(cumprod(f[:2], reverse=True)) == "FExpr<cumprod(f[:2], reverse=True)>"


def test_cumprod_empty_frame():
    DT = dt.Frame()
    expr_cumprod = cumprod(DT)
    assert isinstance(expr_cumprod, FExpr)
    assert_equals(DT[:, cumprod(f[:])], DT)


def test_cumprod_void():
    DT = dt.Frame([None]*10)
    DT_cumprod = DT[:, cumprod(f[:])]
    assert_equals(DT_cumprod, dt.Frame([1]*10/dt.int64))


def test_cumprod_trivial():
    DT = dt.Frame([0]/dt.int64)
    cumprod_fexpr = cumprod(f[:])
    DT_cumprod = DT[:, cumprod_fexpr]
    assert isinstance(cumprod_fexpr, FExpr)
    assert_equals(DT, DT_cumprod)


def test_cumprod_small():
    DT = dt.Frame([range(5), [-1, 1, None, 2, 5.5]])
    DT_cumprod = DT[:, cumprod(f[:])]
    DT_ref = dt.Frame([[0, 0, 0, 0, 0]/dt.int64, [-1, -1, -1, -2, -11]/dt.float64])
    assert_equals(DT_cumprod, DT_ref)

def test_cumprod_reverse():
    DT = dt.Frame([range(5), [-1, 1, None, 2, 5.5]])
    DT_cumprod = DT[:, cumprod(f[:], reverse=True)]
    DT_ref = DT[::-1, cumprod(f[:])][::-1, :]
    assert_equals(DT_cumprod, DT_ref)


def test_cumprod_groupby():
    DT = dt.Frame([[2, 1, 1, 1, 2], [1.5, -1.5, math.inf, 2, 3]])
    DT_cumprod = DT[:, cumprod(f[:]), by(f[0])]
    DT_ref = dt.Frame([[1, 1, 1, 2, 2], [-1.5, -math.inf, -math.inf, 1.5, 4.5]/dt.float64])
    assert_equals(DT_cumprod, DT_ref)

def test_cumprod_groupby_reverse():
    DT = dt.Frame([[2, 1, 1, 1, 2], [1.5, -1.5, math.inf, 2, 3]])
    DT_cumprod = DT[:, cumprod(f[:], reverse=True), by(f[0])]
    DT_ref = dt.Frame([[1, 1, 1, 2, 2], [-math.inf, math.inf, 2.0, 4.5, 3.0]/dt.float64])
    assert_equals(DT_cumprod, DT_ref)

def test_cumprod_void_grouped_column():
    DT = dt.Frame([None]*10)
    DT_cumprod = DT[:, cumprod(f.C0), by(f.C0)]
    assert_equals(DT_cumprod, dt.Frame([[None]*10, [1]*10/dt.int64]))


def test_cumprod_grouped_column():
    DT = dt.Frame([2, 1, None, 1, 2])
    DT_cumprod = DT[:, cumprod(f[0]), by(f[0])]
    DT_ref = dt.Frame([[None, 1, 1, 2, 2], [1, 1, 1, 2, 4]/dt.int64])
    assert_equals(DT_cumprod, DT_ref)


def test_cumprod_groupby_complex():
    DT = dt.Frame([[3, 14, 15, 92, 6], ["a", "cat", "a", "dog", "cat"]])
    DT_cumprod = DT[:, cumprod(f[0].min()), by(f[1])]

    DT_ref = dt.Frame({
               "C1" : ["a", "a", "cat", "cat", "dog"],
               "C0" : [3, 9, 6, 36, 92]/dt.int64
             })
    assert_equals(DT_cumprod, DT_ref)

