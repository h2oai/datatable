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
import math
import pytest
from tests import assert_equals
from datatable import dt, f, join



#-------------------------------------------------------------------------------
# Create column of void type
#-------------------------------------------------------------------------------

def test_create_void0():
    DT = dt.Frame([None])
    assert DT.type == dt.Type.void
    assert DT.shape == (1, 1)


def test_create_void1():
    DT = dt.Frame([None] * 5)
    assert DT.type == dt.Type.void
    assert DT.shape == (5, 1)


def test_create_void2():
    DT = dt.Frame([math.nan] * 15)
    assert DT.type == dt.Type.void
    assert DT.shape == (15, 1)


def test_create_void3():
    # Both `None`s and `nan`s can be used interchangingly
    DT = dt.Frame([None, math.nan, None, None, math.nan])
    assert DT.type == dt.Type.void
    assert DT.shape == (5, 1)
    assert DT.to_list() == [[None] * 5]


def test_create_void_force_type():
    DT = dt.Frame([1, dt, "halp", 4.44, []], type=dt.Type.void)
    assert DT.type == dt.Type.void
    assert DT.shape == (5, 1)
    assert DT.to_list() == [[None] * 5]



#-------------------------------------------------------------------------------
# Joins
#-------------------------------------------------------------------------------

def test_join_void_to_void():
    DT1 = dt.Frame(A=[None, None, None], B=[3, 4, 7])
    DT2 = dt.Frame(A=[None], V=["nothing"])
    DT2.key = "A"
    RES = DT1[:, :, join(DT2)]
    assert_equals(RES, dt.Frame(A=[None, None, None],
                                B=[3, 4, 7],
                                V=["nothing", "nothing", "nothing"]))


@pytest.mark.parametrize('src', [[None, 3, 7, 9],
                                 [math.nan, 2.9, 0.0, -1.5],
                                 [None, 'a', 'b', 'cde']])
def test_join_void_to_any(src):
    DT1 = dt.Frame(A=[None, None, None])
    DT2 = dt.Frame(A=src, V=[10, 20, 30, 40])
    DT2.key = "A"
    RES = DT1[:, :, join(DT2)]
    assert_equals(RES, dt.Frame(A=[None, None, None], V=[10, 10, 10]))


@pytest.mark.parametrize('src', [[None, 13, -5, 902],
                                 [None, 3.14159, 1.2e+100, -1e-5-34],
                                 [math.nan, 'FANG', 'CLAW', 'TALON']])
def test_join_any_to_void(src):
    DT1 = dt.Frame(A=src)
    DT2 = dt.Frame(A=[None], B=['EM-cah-too'])
    DT2.key = "A"
    RES = DT1[:, :, join(DT2)]
    assert_equals(RES, dt.Frame(A=src, B=['EM-cah-too', None, None, None]))



#-------------------------------------------------------------------------------
# Save to ...
#-------------------------------------------------------------------------------

def test_to_jay():
    DT1 = dt.Frame([None] * 13)
    saved = DT1.to_jay()
    DT2 = dt.fread(saved)
    assert DT2.type == dt.Type.void
    assert_equals(DT1, DT2)


def test_to_csv():
    DT = dt.Frame([None] * 9)
    out = DT.to_csv()
    assert out == "C0\n" + "\n" * 9


def test_view_to_jay():
    # See issue #3099
    DT1 = dt.Frame([None] * 10)[:3, :]
    saved = DT1.to_jay()
    DT2 = dt.fread(saved)
    assert DT2.type == dt.Type.void
    assert_equals(DT1, DT2)


def test_view_to_jay_2():
    DT1 = dt.Frame(A=[None, None], B=[None, "qoo"])[0, :]
    saved = DT1.to_jay()
    DT2 = dt.fread(saved)
    assert_equals(DT2, dt.Frame(A=[None], B=[None]/dt.str32))



#-------------------------------------------------------------------------------
# Misc
#-------------------------------------------------------------------------------

def test_materialize():
    DT = dt.Frame([None] * 5)
    DT.materialize()
    assert DT.type == dt.Type.void
    assert DT.to_list() == [[None] * 5]


def test_sort_void_simple1():
    DT = dt.Frame([None] * 111)
    DT.sort(0)
    assert DT.type == dt.Type.void
    assert DT.to_list() == [[None] * 111]


def test_sort_void_simple2():
    DT = dt.Frame([None] * 2345)
    assert_equals(DT[:, :, dt.sort(f[0])], DT)


def test_sort_void_multi1():
    DT = dt.Frame(A=[None] * 10)
    DT.sort(f.A, f[0])
    assert DT.type == dt.Type.void
    assert DT.to_list() == [[None] * 10]


def test_sort_void_multi2():
    DT = dt.Frame(A=[None] * 5, B=[3, 8, 1, 0, 2])
    RES = DT[:, :, dt.sort(f.A, f.B)]
    EXP = dt.Frame(A=[None] * 5, B=[0, 1, 2, 3, 8])
    assert_equals(RES, EXP)


def test_groupby_void():
    DT = dt.Frame([None] * 5)[:, dt.count(), dt.by(0)]
    assert_equals(DT, dt.Frame(C0=[None], count=[5]/dt.int64))


def test_groupby_void2():
    # See issue #3104
    DT0 = dt.Frame(A=[None] * 5, B=range(5), C=['q'] * 5)
    DT1 = DT0[:, dt.count(), dt.by(f.A, f.B)]
    EXP = dt.Frame(A=[None]*5, B=range(5), count=([1] * 5)/dt.int64)
    assert_equals(DT1, EXP)
