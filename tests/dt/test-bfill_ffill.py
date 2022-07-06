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
from datatable import dt, f, bfill, ffill, FExpr, by
from tests import assert_equals


#-------------------------------------------------------------------------------
# Errors
#-------------------------------------------------------------------------------
@pytest.mark.xfail(reason='remove later')
@pytest.mark.parametrize("mm", [ffill, bfill])
# get rid of this once strings are sorted out
def test_ffillmax_non_numeric(mm):
    DT = dt.Frame(list('abcde'))
    with pytest.raises(TypeError,
                       match = r'Invalid column of type str32 in ' + mm.__name__):
        DT[:, mm(f[0])]

@pytest.mark.xfail(reason='remove later')
@pytest.mark.parametrize("mm", [ffill, bfill])
# same here
def test_ffillmax_non_numeric_by(mm):
    DT = dt.Frame(list('abcde'))
    with pytest.raises(TypeError,
                       match = r'Invalid column of type str32 in ' + mm.__name__):
        DT[:, mm(f[0]), by(f[0])]


@pytest.mark.parametrize("mm", [ffill, bfill])
def test_ffillmax_no_argument(mm):
    msg = (f"Function datatable.{mm.__name__}"
           "\\(\\) requires exactly 1 positional argument, but none were given")
    with pytest.raises(TypeError, match = msg):
        mm()


#-------------------------------------------------------------------------------
# Normal
#-------------------------------------------------------------------------------

@pytest.mark.parametrize("mm", [ffill, bfill])
def test_ffillmax_str(mm):
  assert str(mm(f.A)) == "FExpr<" + mm.__name__ + "(f.A)>"
  assert str(mm(f.A) + 1) == "FExpr<" + mm.__name__ + "(f.A) + 1>"
  assert str(mm(f.A + f.B)) == "FExpr<" + mm.__name__ + "(f.A + f.B)>"
  assert str(mm(f.B)) == "FExpr<" + mm.__name__ + "(f.B)>"
  assert str(mm(f[:2])) == "FExpr<"+ mm.__name__ + "(f[:2])>"


@pytest.mark.parametrize("mm", [ffill, bfill])
def test_fill_empty_frame(mm):
    DT = dt.Frame()
    expr_mm = mm(DT)
    assert isinstance(expr_mm, FExpr)
    assert_equals(DT[:, mm(f[:])], DT)


@pytest.mark.parametrize("mm", [ffill, bfill])
def test_fill_void(mm):
    DT = dt.Frame([None, None, None])
    DT_mm = DT[:, mm(f[:])]
    assert_equals(DT_mm, DT)


@pytest.mark.parametrize("mm", [ffill, bfill])
def test_fill_trivial(mm):
    DT = dt.Frame([0]/dt.int64)
    mm_fexpr = mm(f[:])
    DT_mm = DT[:, mm_fexpr]
    assert isinstance(mm_fexpr, FExpr)
    assert_equals(DT, DT_mm)


def test_fill_bool():
    DT = dt.Frame([None, False, None, True, False, True])
    DT_mm = DT[:, [ffill(f[:]), bfill(f[:])]]
    DT_ref = dt.Frame([
                [None, False, False, True, False, True],
                [False, False, True, True, False, True]
             ])
    assert_equals(DT_mm, DT_ref)


def test_fill_small():
    DT = dt.Frame([None, 3, None, 4])
    DT_bfill = DT[:, [ffill(f[:]), bfill(f[:])]]
    DT_ref = dt.Frame([
                 [None, 3, 3, 4],
                 [3, 3, 4, 4]
             ])
    assert_equals(DT_bfill, DT_ref)


def test_fill_groupby():
    DT = dt.Frame([[15, None, 136, 93, 743, None, None, 91], ['a','a','a','b','b','c','c','c']])
    DT_bfill = DT[:, [ffill(f[:]), bfill(f[:])], by(f[-1])]
    DT_ref = dt.Frame({
                 'C1':['a','a','a','b','b','c','c','c'],
                 'C0':[15, 15, 136, 93, 743, None, None, 91],
                 'C2':[15, 136, 136, 93, 743, 91, 91, 91],
             })
    assert_equals(DT_bfill, DT_ref)


def test_fill_grouped_column():
    DT = dt.Frame([2, 1, None, 1, 2])
    DT_bfill = DT[:, [ffill(f[0]), bfill(f[0])], by(f[0])]
    DT_ref = dt.Frame([
                [None, 1, 1, 2, 2],
                [None, 1, 1, 2, 2],
                [None, 1, 1, 2, 2]
             ])
    assert_equals(DT_bfill, DT_ref)


