#!/usr/bin/env python
# -*- coding: utf-8 -*-
#-------------------------------------------------------------------------------
# Copyright 2018-2019 H2O.ai
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
import datatable as dt
from datatable import f, shift, by
from tests import assert_equals


#-------------------------------------------------------------------------------
# shift()
#-------------------------------------------------------------------------------

def test_shift_default():
    DT = dt.Frame(A=range(5))
    assert_equals(DT[:, shift(f.A)],
                  dt.Frame(A=[None, 0, 1, 2, 3]))


def test_shift_frame():
    DT = dt.Frame(A=range(5))
    RES = shift(DT, 2)
    assert_equals(RES, dt.Frame(A=[None, None, 0, 1, 2]))


def test_shift_amount():
    DT = dt.Frame(range(5))
    RES = DT[:, [shift(f.C0, n) for n in range(-5, 6)]]
    assert_equals(RES,
        dt.Frame([[None, None, None, None, None],
                  [4, None, None, None, None],
                  [3, 4, None, None, None],
                  [2, 3, 4, None, None],
                  [1, 2, 3, 4, None],
                  [0, 1, 2, 3, 4],
                  [None, 0, 1, 2, 3],
                  [None, None, 0, 1, 2],
                  [None, None, None, 0, 1],
                  [None, None, None, None, 0],
                  [None, None, None, None, None]], stype=dt.int32))


def test_shift_stypes():
    DT = dt.Frame([[0, 1, 2, 3, 4],
                   [2.7, 9.4, -1.1, None, 3.4],
                   ["one", "two", "three", "FOUR", "five"],
                   [True, False, True, True, False]])
    RES = shift(DT, n=1)
    assert_equals(RES,
        dt.Frame([[None, 0, 1, 2, 3],
                  [None, 2.7, 9.4, -1.1, None],
                  [None, "one", "two", "three", "FOUR"],
                  [None, True, False, True, True]]))


def test_shift_all_columns():
    DT = dt.Frame([[5, 9, 1], list("ABC"), [2.3, 4.4, -2.5]],
                  names=["A", "B", "D"])
    RES = DT[:, shift(f[:], n=1)]
    assert_equals(RES, dt.Frame(A=[None, 5, 9],
                                B=[None, "A", "B"],
                                D=[None, 2.3, 4.4]))


def test_shift_expr():
    DT = dt.Frame(A=[3, 4, 5, 6], B=[-1, 2, -2, 3])
    RES = DT[:, shift(f.A + f.B, n=1)]
    assert_equals(RES, dt.Frame([None, 2, 6, 3]))




#-------------------------------------------------------------------------------
# shift() with groupby
#-------------------------------------------------------------------------------

def test_shift_with_by():
    DT = dt.Frame(A=[1, 2, 1, 1, 2, 1, 2],
                  B=[3, 7, 9, 0, -1, 2, 1])
    RES = DT[:, {"lag1": shift(f.B, 1),
                 "lag2": shift(f.B, 2),
                 "lag3": shift(f.B, 3),
                 "nolag": shift(f.B, 0),
                 "lead1": shift(f.B, -1),
                 "lead2": shift(f.B, -2),
                 "lead3": shift(f.B, -3),
                 }, by(f.A)]
    assert_equals(RES, dt.Frame(A=[1, 1, 1, 1, 2, 2, 2],
                                lag1=[None, 3, 9, 0, None, 7, -1],
                                lag2=[None, None, 3, 9, None, None, 7],
                                lag3=[None, None, None, 3, None, None, None],
                                nolag=[3, 9, 0, 2, 7, -1, 1],
                                lead1=[9, 0, 2, None, -1, 1, None],
                                lead2=[0, 2, None, None, 1, None, None],
                                lead3=[2, None, None, None, None, None, None]))


def test_shift_group_column():
    DT = dt.Frame(A=[1, 2, 1, 1, 2])
    RES = DT[:, shift(f.A), by(f.A)]
    assert_equals(RES, dt.Frame({"A": [1, 1, 1, 2, 2],
                                 "A.0": [None, 1, 1, None, 2]}))


def test_shift_reduced_column():
    DT = dt.Frame(A=[1, 2, 1, 1, 2, 1], B=range(6))
    RES = DT[:, shift(dt.sum(f.B)), by(f.A)]
    assert_equals(RES, dt.Frame(A=[1, 1, 1, 1, 2, 2],
                                B=[None, 10, 10, 10, None, 5],
                                stypes={"A": dt.int32, "B": dt.int64}))



def test_shift_by_with_i():
    DT = dt.Frame(A=[1, 2, 1, 2, 1, 2, 1, 2], B=range(8))
    RES = DT[1:, shift(f.B), by(f.A)]
    assert_equals(RES, dt.Frame(A=[1, 1, 1, 2, 2, 2],
                                B=[None, 2, 4, None, 3, 5]))




#-------------------------------------------------------------------------------
# Test errors
#-------------------------------------------------------------------------------

def test_shift_wrong_signature1():
    msg = r"Function `shift\(\)` requires 1 positional argument"
    with pytest.raises(TypeError, match=msg):
        shift()
    with pytest.raises(TypeError, match=msg):
        shift(None)
    with pytest.raises(TypeError, match=msg):
        shift(n=3)


def test_shift_wrong_signature2():
    msg = r"The first argument to `shift\(\)` must be a column expression " \
          r"or a Frame"
    for s in [3, 12.5, "hi", dt]:
        with pytest.raises(TypeError, match=msg):
            shift(s)


def test_shift_wrong_signature3():
    msg = r"Argument `n` in shift\(\) should be an integer"
    for n in ["one", 0.0, f.B, range(3), [1, 2, 3]]:
        with pytest.raises(TypeError, match=msg):
            shift(f.A, n=n)
