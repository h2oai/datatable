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
import pytest
from datatable import dt, f, nth, FExpr, by
from tests import assert_equals

#-------------------------------------------------------------------------------
# Errors
#-------------------------------------------------------------------------------

def test_nth_parameter_not_int():
    msg = r"Argument n in function datatable.nth\(\) should be an integer, " \
          "instead got <class 'str'>"
    DT = dt.Frame([1, 2, None, 4, 5])
    with pytest.raises(TypeError, match = msg):
        DT[:, nth(f[0], '1')]


def test_nth_no_argument():
    msg = r"Function datatable.nth\(\) requires at least 1 positional " \
          "argument, but none were given"
    with pytest.raises(TypeError, match = msg):
        nth()


#-------------------------------------------------------------------------------
# Normal
#-------------------------------------------------------------------------------

def test_nth_str():
  assert str(nth(f.A, nth=1)) == "FExpr<" + nth.__name__ + "(f.A, nth=1, skipna=False)>"
  assert str(nth(f.A, nth=1, skipna=True) + 1) == "FExpr<" + nth.__name__ + "(f.A, nth=1, skipna=True) + 1>"
  assert str(nth(f.A + f.B, nth = 1)) == "FExpr<" + nth.__name__ + "(f.A + f.B, nth=1, skipna=False)>"
  assert str(nth(f.B, 1, True)) == "FExpr<" + nth.__name__ + "(f.B, nth=1, skipna=True)>"
  assert str(nth(f[:2], 1)) == "FExpr<"+ nth.__name__ + "(f[:2], nth=1, skipna=False)>"


def test_nth_empty_frame():
    DT = dt.Frame()
    expr_nth = nth(DT, 1)
    assert isinstance(expr_nth, FExpr)
    assert_equals(DT[:, nth(f[:], 1)], DT)


def test_nth_empty_frame_skipna():
    DT = dt.Frame()
    expr_nth = nth(DT, 1)
    assert isinstance(expr_nth, FExpr)
    assert_equals(DT[:, nth(f[:], 1)], DT)


def test_nth_void():
    DT = dt.Frame([None, None, None])
    DT_nth = DT[:, nth(f[:], 0)]
    assert_equals(DT_nth, DT[0, :])
    
def test_nth_void_skipna():
    DT = dt.Frame([None, None, None])
    DT_nth = DT[:, nth(f[:], 0, True)]
    assert_equals(DT_nth, DT[0, :])


def test_nth_trivial():
    DT = dt.Frame([0]/dt.int64)
    nth_fexpr = nth(f[:], n=-1)
    DT_nth = DT[:, nth_fexpr]
    assert isinstance(nth_fexpr, FExpr)
    assert_equals(DT, DT_nth)


def test_nth_trivial_skipna():
    DT = dt.Frame([0]/dt.int64)
    nth_fexpr = nth(f[:], nth = -1, skipna=True)
    DT_nth = DT[:, nth_fexpr]
    assert isinstance(nth_fexpr, FExpr)
    assert_equals(DT, DT_nth)
    

def test_nth_bool():
    DT = dt.Frame([None, False, None, True, False, True])
    DT_nth = DT[:, [nth(f[:], n=1),
                    nth(f[:], n=-1),
                    nth(f[:], n=24)]]
    DT_ref = dt.Frame([[False], [True], [None]/dt.bool8])
    assert_equals(DT_nth, DT_ref)


def test_nth_bool_skipna():
    DT = dt.Frame([None, False, None, True, False, True])
    DT_nth = DT[:, [nth(f[:], nth = 0, skipna=True),
                    nth(f[:], nth = -1, skipna=True),
                    nth(f[:], nth = 2,skipna=True)]]
    DT_ref = dt.Frame([[False], [True], [True]])

    assert_equals(DT_nth, DT_ref)
    
    

def test_nth_small():
    DT = dt.Frame([None, 3, None, 4])
    DT_nth = DT[:, [nth(f[:], n=1),
                    nth(f[:], n=-5)]]
    DT_ref = dt.Frame([[3]/dt.int32, [None]/dt.int32])
    assert_equals(DT_nth, DT_ref)


def test_nth_string():
    DT = dt.Frame(['d', 'a', 'z', 'b'])
    DT_nth = DT[:, [nth(f[:], 0), nth(f[:], n=-1)]]
    DT_ref = dt.Frame([['d'], ['b'] ])
    assert_equals(DT_nth, DT_ref)


def test_nth_grouped():
    DT = dt.Frame([[15, None, 136, 93, 743, None, None, 91],
                  ['a','a','a','b','b','c','c','c']])
    DT_nth = DT[:, [nth(f[:], n=0), nth(f[:], n=2)], by(f[-1])]
    DT_ref = dt.Frame({
                'C1':['a','b','c',],
                'C0':[15, 93, None],
                'C2':[136, None, 91 ],
             })
    assert_equals(DT_nth, DT_ref)


def test_nth_grouped_skipna():
    DT = dt.Frame([[15, None, 136, 93, 743, None, None, 91],
                  ['a','a','a','b','b','c','c','c']])
    DT_nth = DT[:, [nth(f[:], nth = 0, skipna=True), nth(f[:], nth = 2,skipna=True)], by(f[-1])]
    DT_ref = dt.Frame({
                'C1':['a','b','c',],
                'C2':[15,93, 91],
                'C3':[136, None,91 ],
             })
    assert_equals(DT_nth, DT_ref)
    
