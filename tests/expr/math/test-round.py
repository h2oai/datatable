#!/usr/bin/env python
# -*- coding: utf-8 -*-
#-------------------------------------------------------------------------------
# Copyright 2020 H2O.ai
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
from datatable import dt, f
from datatable.math import round as dtround
from tests import assert_equals


def test_round_noargs():
    msg = r"Function datatable.round\(\) requires exactly 1 positional " \
          r"argument, but none were given"
    with pytest.raises(TypeError, match=msg):
        assert dtround()


def test_round_ndigits_expr():
    msg = r"Argument ndigits in function datatable.round\(\) should be an " \
          r"integer, instead got <class 'datatable.FExpr'>"
    with pytest.raises(TypeError, match=msg):
        assert dtround(f.A, ndigits=f.B)


def test_round_expr_instance():
    assert isinstance(dtround(f.A), dt.FExpr)
    assert isinstance(dtround(2.5), dt.FExpr)
    assert isinstance(dtround(2.5, ndigits=1), dt.FExpr)


def test_round_expr_str():
    assert str(dtround(1)) == "FExpr<round(1)>"
    assert str(dtround(f.A)) == "FExpr<round(f.A)>"
    assert str(dtround(f.A, ndigits=None)) == "FExpr<round(f.A)>"
    assert str(dtround(f.A, ndigits=0)) == "FExpr<round(f.A, ndigits=0)>"
    assert str(dtround(f[:], ndigits=-5)) == "FExpr<round(f[:], ndigits=-5)>"



#-------------------------------------------------------------------------------
# SType::BOOL
#-------------------------------------------------------------------------------

def test_round_bool_positive_ndigits():
    DT = dt.Frame(A=[True, False, None])
    assert_equals(DT[:, dtround(f.A)], DT)
    assert_equals(DT[:, dtround(f.A, ndigits=0)], DT)
    assert_equals(DT[:, dtround(f.A, ndigits=1)], DT)
    assert_equals(DT[:, dtround(f.A, ndigits=2)], DT)
    assert_equals(DT[:, dtround(f.A, ndigits=3)], DT)
    assert_equals(DT[:, dtround(f.A, ndigits=5)], DT)
    assert_equals(DT[:, dtround(f.A, ndigits=9)], DT)
    assert_equals(DT[:, dtround(f.A, ndigits=19)], DT)
    assert_equals(DT[:, dtround(f.A, ndigits=999999)], DT)


def test_round_bool_negative_ndigits():
    DT = dt.Frame(A=[True, False, None])
    D0 = dt.Frame(A=[False, False, False])
    assert_equals(DT[:, dtround(f.A, ndigits=-1)], D0)
    assert_equals(DT[:, dtround(f.A, ndigits=-2)], D0)
    assert_equals(DT[:, dtround(f.A, ndigits=-3)], D0)
    assert_equals(DT[:, dtround(f.A, ndigits=-7)], D0)
    assert_equals(DT[:, dtround(f.A, ndigits=-19)], D0)
    assert_equals(DT[:, dtround(f.A, ndigits=-1234567)], D0)



#-------------------------------------------------------------------------------
# SType::INT8
#-------------------------------------------------------------------------------

def test_round_int8_positive_ndigits():
    DT = dt.Frame(A=[None] + list(range(-127, 128)), stype=dt.int8)
    assert_equals(DT[:, dtround(f.A)], DT)
    assert_equals(DT[:, dtround(f.A, ndigits=0)], DT)
    assert_equals(DT[:, dtround(f.A, ndigits=1)], DT)
    assert_equals(DT[:, dtround(f.A, ndigits=2)], DT)
    assert_equals(DT[:, dtround(f.A, ndigits=3)], DT)
    assert_equals(DT[:, dtround(f.A, ndigits=5)], DT)
    assert_equals(DT[:, dtround(f.A, ndigits=9)], DT)
    assert_equals(DT[:, dtround(f.A, ndigits=17)], DT)
    assert_equals(DT[:, dtround(f.A, ndigits=987654321)], DT)


@pytest.mark.parametrize('nd', [-1, -2])
def test_round_int8_negative_ndigits_small(nd):
    DT = dt.Frame(A=[None] + list(range(-127, 128)), stype=dt.int8)
    RES = DT[:, dtround(f.A, ndigits=nd)]
    EXP = dt.Frame(A=[None] + [round(x, nd) for x in range(-127, 128)],
                   stype=dt.int8)
    assert_equals(RES, EXP)


@pytest.mark.parametrize('nd', [-3, -5, -17])
def test_round_int8_negative_ndigits_large(nd):
    DT = dt.Frame(A=[None] + list(range(-127, 128)), stype=dt.int8)
    RES = DT[:, dtround(f.A, ndigits=nd)]
    assert_equals(RES, dt.Frame(A=[0] * 256 / dt.int8))



#-------------------------------------------------------------------------------
# SType::INT16
#-------------------------------------------------------------------------------

def test_round_int16_positive_ndigits():
    DT = dt.Frame(A=[None, 12, 0, 34, -999, 32767, 10001, -32767] / dt.int16)
    assert_equals(DT[:, dtround(f.A)], DT)
    assert_equals(DT[:, dtround(f.A, ndigits=0)], DT)
    assert_equals(DT[:, dtround(f.A, ndigits=1)], DT)
    assert_equals(DT[:, dtround(f.A, ndigits=2)], DT)
    assert_equals(DT[:, dtround(f.A, ndigits=3)], DT)
    assert_equals(DT[:, dtround(f.A, ndigits=5)], DT)
    assert_equals(DT[:, dtround(f.A, ndigits=9)], DT)
    assert_equals(DT[:, dtround(f.A, ndigits=17)], DT)
    assert_equals(DT[:, dtround(f.A, ndigits=314)], DT)


@pytest.mark.parametrize('nd', [-1, -2, -3, -4])
def test_round_int16_negative_ndigits_small(nd):
    src = [1, 3, 5, 6, 9, 12, 15, 20, 25, 35, 34, 45, 95, 1115, 11115, 0,
           -5, -15, -25, -24, -26, -999, -9999, -1005, -1015, -10005, -32767,
           32767, 32760, 32765, 10001, 10003]
    DT = dt.Frame(src / dt.int16)
    RES = DT[:, dtround(f[0], ndigits=nd)]
    EXP = dt.Frame([round(x, nd) for x in src] / dt.int16)
    assert_equals(RES, EXP)



#-------------------------------------------------------------------------------
# SType::INT32
#-------------------------------------------------------------------------------

def test_round_int32_positive_ndigits():
    m = 2**31 - 1
    DT = dt.Frame(A=[None, 12, 0, 34, -999, m, 15, 1001, 123456, -m])
    assert DT.stype == dt.int32
    assert_equals(DT[:, dtround(f.A)], DT)
    assert_equals(DT[:, dtround(f.A, ndigits=0)], DT)
    assert_equals(DT[:, dtround(f.A, ndigits=1)], DT)
    assert_equals(DT[:, dtround(f.A, ndigits=2)], DT)
    assert_equals(DT[:, dtround(f.A, ndigits=3)], DT)
    assert_equals(DT[:, dtround(f.A, ndigits=5)], DT)
    assert_equals(DT[:, dtround(f.A, ndigits=9)], DT)
    assert_equals(DT[:, dtround(f.A, ndigits=17)], DT)
    assert_equals(DT[:, dtround(f.A, ndigits=314)], DT)


@pytest.mark.parametrize('nd', [-1, -2, -3, -4, -5, -6, -7, -8, -9])
def test_round_int32_negative_ndigits_small(nd):
    src = [1, 3, 5, 6, 9, 12, 15, 20, 25, 35, 34, 45, 95, 1115, 11115, 0,
           -5, -15, -25, -24, -26, -999, -9999, -1005, -1015, -10005, -32767,
           32767, 32760, 32765, 10001, 10003, 1000005, 3017097, 3198734,
           34982340, 34982345, 34982355, 34982365, 34982375, 34982385]
    DT = dt.Frame(src / dt.int32)
    RES = DT[:, dtround(f[0], ndigits=nd)]
    EXP = dt.Frame([round(x, nd) for x in src] / dt.int32)
    assert_equals(RES, EXP)


@pytest.mark.parametrize('nd', [-10, -11, -13, -997])
def test_round_int32_negative_ndigits_large(nd):
    src = [None, 0, 1, 12, 34083, 1082745, 59845702, 2**31-1, 1-2**31]
    DT = dt.Frame(src / dt.int32)
    RES = DT[:, dtround(f[0], ndigits=nd)]
    EXP = dt.Frame([0] * len(src) / dt.int32)
    assert_equals(RES, EXP)



#-------------------------------------------------------------------------------
# SType::INT64
#-------------------------------------------------------------------------------

def test_round_int64_positive_ndigits():
    m = 2**63 - 1
    DT = dt.Frame(A=[None, 12, 0, 34, -999, m, 15, 1001, 123456, -m])
    assert DT.stype == dt.int64
    assert_equals(DT[:, dtround(f.A)], DT)
    assert_equals(DT[:, dtround(f.A, ndigits=0)], DT)
    assert_equals(DT[:, dtround(f.A, ndigits=1)], DT)
    assert_equals(DT[:, dtround(f.A, ndigits=2)], DT)
    assert_equals(DT[:, dtround(f.A, ndigits=3)], DT)
    assert_equals(DT[:, dtround(f.A, ndigits=5)], DT)
    assert_equals(DT[:, dtround(f.A, ndigits=9)], DT)
    assert_equals(DT[:, dtround(f.A, ndigits=17)], DT)
    assert_equals(DT[:, dtround(f.A, ndigits=314)], DT)


@pytest.mark.parametrize('nd', [-1, -2, -3, -4, -7, -9, -12, -16, -17, -19])
def test_round_int32_negative_ndigits_small(nd):
    src = [1, 3, 5, 6, 9, 12, 15, 20, 25, 35, 34, 45, 95, 1115, 11115, 0,
           -5, -15, -25, -24, -26, -999, -9999, -1005, -1015, -10005, -32767,
           32767, 32760, 32765, 10001, 10003, 1000005, 3017097, 3198734,
           349823445870, 34982341365, 3498223675355, 3493497682365,
           349346782375, 33464982385]
    DT = dt.Frame(src / dt.int64)
    RES = DT[:, dtround(f[0], ndigits=nd)]
    EXP = dt.Frame([round(x, nd) for x in src] / dt.int64)
    assert_equals(RES, EXP)


@pytest.mark.parametrize('nd', [-20, -21, -1337])
def test_round_int32_negative_ndigits_large(nd):
    src = [None, 1, -1, 12, 34083, 123082745, 5123659845702, 2**63-1, 1-2**63]
    DT = dt.Frame(src / dt.int64)
    RES = DT[:, dtround(f[0], ndigits=nd)]
    EXP = dt.Frame([0] * len(src) / dt.int64)
    assert_equals(RES, EXP)



#-------------------------------------------------------------------------------
# SType::FLOAT32
#-------------------------------------------------------------------------------

def test_round_float32_no_ndigits():
    DT = dt.Frame(A=[1.5, 12.3, 2.5, 7.7, 4.5, 6.5, None] / dt.float32)
    RES = DT[:, dtround(f.A)]
    EXP = dt.Frame(A=[2, 12, 2, 8, 4, 6, None] / dt.int64)
    assert_equals(RES, EXP)


def test_round_float32_0_ndigits():
    DT = dt.Frame(A=[1.5, 12.3, 2.5, 7.7, 4.5, 6.5, None] / dt.float32)
    RES = DT[:, dtround(f.A, ndigits=0)]
    EXP = dt.Frame(A=[2, 12, 2, 8, 4, 6, None] / dt.float32)
    assert_equals(RES, EXP)



#-------------------------------------------------------------------------------
# SType::FLOAT64
#-------------------------------------------------------------------------------

def test_round_float64_no_ndigits():
    DT = dt.Frame(A=[1.5, 12.3, 2.5, 7.7, 4.5, 6.5, None])
    RES = DT[:, dtround(f.A)]
    EXP = dt.Frame(A=[2, 12, 2, 8, 4, 6, None] / dt.int64)
    assert_equals(RES, EXP)


def test_round_float64_0_ndigits():
    DT = dt.Frame(A=[1.5, 12.3, 2.5, 7.7, 4.5, 6.5, None])
    RES = DT[:, dtround(f.A, ndigits=0)]
    EXP = dt.Frame(A=[2, 12, 2, 8, 4, 6, None] / dt.float64)
    assert_equals(RES, EXP)


@pytest.mark.parametrize("seed", [random.getrandbits(32)])
def test_round_float64_random(seed):
    random.seed(seed)
    n = 1000
    src = [random.random() * 1000 for i in range(n)]
    ndigits = random.randint(-3, 10)
    DT = dt.Frame(src)
    RES = DT[:, dtround(f[0], ndigits=ndigits)]
    EXP = dt.Frame([round(x, ndigits) for x in src])
    assert_equals(RES, EXP)



#-------------------------------------------------------------------------------
# other
#-------------------------------------------------------------------------------

@pytest.mark.parametrize("st", [dt.str32, dt.str64, dt.obj64])
def test_round_string(st):
    msg = r"Function datatable.math.round\(\) cannot be applied to a column " \
          r"of type " + st.name
    with pytest.raises(TypeError, match=msg):
        assert dt.Frame(['a', 'b', 'c'] / st)[:, dtround(f[0])]
