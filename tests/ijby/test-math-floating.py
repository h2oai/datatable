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
from datatable import f, g, stype, ltype, join
from datatable.internal import frame_integrity_check
from tests import assert_equals


# Sets of tuples containing test columns of each type
srcs_bool = [[False, True, False, False, True],
             [True, None, None, True, False]]

srcs_int = [[5, -3, 6, 3, 0],
            [None, -1, 0, 26, -3],
            [2**31 - 2, -(2**31 - 1), 0, -1, 1]]

srcs_float = [[9.5, 0.2, 5.4857301, -3.14159265338979],
              [1.1, 2.3e12, -.5, None, math.inf, 0.0]]

srcs_str = [["foo", "bbar", "baz"],
            [None, "", " ", "  ", None, "\x00"],
            list("qwertyuiiop[]asdfghjkl;'zxcvbnm,./`1234567890-=")]



#-------------------------------------------------------------------------------
# math.abs()
#-------------------------------------------------------------------------------

def test_abs():
    assert dt.math.abs(True) == 1
    assert dt.math.abs(1) == 1
    assert dt.math.abs(-5) == 5
    assert dt.math.abs(-2.5e12) == 2.5e12
    assert dt.math.abs(math.nan) is None
    assert dt.math.abs(None) is None


@pytest.mark.parametrize("src", srcs_bool + srcs_int + srcs_float)
def test_abs_srcs(src):
    DT = dt.Frame(src)
    RES = dt.math.abs(DT)
    target_stype = dt.int32 if DT.stype == dt.bool8 else DT.stype
    assert_equals(RES, dt.Frame([None if x is None else abs(x) for x in src],
                                stype=target_stype))


def test_abs_all_stypes():
    src = [[-127, -5, -1, 0, 2, 127],
           [-32767, -299, -7, 32767, 12, -543],
           [-2147483647, -1000, 3, -589, 2147483647, 0],
           [-2**63 + 1, 2**63 - 1, 0, -2**32, 2**32, -793]]
    DT = dt.Frame(src, stypes=[dt.int8, dt.int16, dt.int32, dt.int64])
    RES = DT[:, dt.math.abs(f[:])]
    assert_equals(RES,
                  dt.Frame([[abs(x) for x in col] for col in src],
                           stypes=[dt.int32, dt.int32, dt.int32, dt.int64]))


def test_abs_wrong_type():
    msg = "Function `abs` cannot be applied to a column of type `str32`"
    with pytest.raises(TypeError, match=msg):
        DT = dt.Frame(['foo'])
        assert DT[:, dt.math.abs(f.C0)]




#-------------------------------------------------------------------------------
# math.ceil()
#-------------------------------------------------------------------------------

def test_ceil():
    assert dt.math.ceil(1) == 1.0
    assert isinstance(dt.math.ceil(1), float)
    assert dt.math.ceil(True) == 1.0
    assert dt.math.ceil(3.5) == 4.0
    assert dt.math.ceil(1e100) == 1e100
    assert dt.math.ceil(None) is None
    assert dt.math.ceil(math.nan) is None
    with pytest.raises(TypeError):
        assert dt.math.ceil("hello")


@pytest.mark.parametrize("src", srcs_bool + srcs_int + srcs_float)
def test_ceil_srcs(src):
    DT = dt.Frame(src)
    RES = dt.math.ceil(DT)
    assert_equals(RES, dt.Frame([None if x is None else
                                 x if x == math.inf or x == -math.inf else
                                 math.ceil(x)
                                 for x in src],
                                stype=dt.float64))


def test_ceil_wrong_type():
    msg = "Function `ceil` cannot be applied to a column of type `str32`"
    with pytest.raises(TypeError, match=msg):
        DT = dt.Frame(['foo'])
        assert DT[:, dt.math.ceil(f.C0)]


def test_ceil_random(numpy):
    arr = numpy.random.randn(100)
    RES_DT = dt.math.ceil(dt.Frame(arr))
    RES_NP = numpy.ceil(arr)
    assert_equals(RES_DT, dt.Frame(RES_NP))




#-------------------------------------------------------------------------------
# math.copysign()
#-------------------------------------------------------------------------------

def test_copysign():
    DT = dt.Frame(A=[3.4, -7.2, -9.9, 0.1, 12.99, None, 1.2],
                  B=[1, 3, 999, -1, -7, 4, None])
    RES = DT[:, dt.math.copysign(f.A, f.B)]
    assert_equals(RES, dt.Frame([3.4, 7.2, 9.9, -0.1, -12.99, None, None]))


def test_copysign_constant():
    DT = dt.Frame(A=[3.1, -1.1, 2.4, -0.0, None, -math.inf])
    RES = DT[:, dt.math.copysign(1, f.A)]
    assert_equals(RES, dt.Frame([1.0, -1.0, 1.0, -1.0, None, -1.0]))


def test_copysign_wrong_signature():
    msg1 = r"Function `copysign\(\)` takes 2 positional arguments"
    with pytest.raises(TypeError, match=msg1):
        dt.math.copysign(f[7])

    msg2 = r"Cannot apply function `copysign\(\)` to columns with types " \
           r"`str32` and `str32`"
    with pytest.raises(TypeError, match=msg2):
        dt.Frame(A=['hello'])[:, dt.math.copysign(f.A, f.A)]




#-------------------------------------------------------------------------------
# math.fabs()
#-------------------------------------------------------------------------------

def test_fabs():
    assert isinstance(dt.math.fabs(1), float)
    assert dt.math.fabs(1) == 1.0
    assert dt.math.fabs(-3.3) == 3.3
    assert dt.math.fabs(True) == 1.0
    assert dt.math.fabs(-math.inf) == math.inf
    assert dt.math.fabs(None) is None


@pytest.mark.parametrize("src", srcs_bool + srcs_int + srcs_float)
def test_fabs_srcs(src):
    DT = dt.Frame(src)
    RES = dt.math.fabs(DT)
    assert_equals(RES, dt.Frame([None if x is None else
                                 abs(x) for x in src],
                                stype=dt.float64))


def test_fabs_random(numpy):
    arr = numpy.random.randn(100)
    RES_DT = dt.math.fabs(dt.Frame(arr))
    RES_NP = numpy.fabs(arr)
    assert_equals(RES_DT, dt.Frame(RES_NP))




#-------------------------------------------------------------------------------
# math.floor()
#-------------------------------------------------------------------------------

def test_floor():
    assert dt.math.floor(1) == 1.0
    assert isinstance(dt.math.floor(1), float)
    assert dt.math.floor(True) == 1.0
    assert dt.math.floor(3.5) == 3.0
    assert dt.math.floor(1e100) == 1e100
    assert dt.math.floor(None) is None
    assert dt.math.floor(math.nan) is None
    with pytest.raises(TypeError):
        assert dt.math.floor("bye")


@pytest.mark.parametrize("src", srcs_bool + srcs_int + srcs_float)
def test_floor_srcs(src):
    DT = dt.Frame(src)
    RES = dt.math.floor(DT)
    assert_equals(RES, dt.Frame([None if x is None else
                                 x if x == math.inf or x == -math.inf else
                                 math.floor(x)
                                 for x in src],
                                stype=dt.float64))


def test_floor_wrong_type():
    msg = "Function `floor` cannot be applied to a column of type `str32`"
    with pytest.raises(TypeError, match=msg):
        DT = dt.Frame(['foo'])
        assert DT[:, dt.math.floor(f.C0)]


def test_floor_random(numpy):
    arr = numpy.random.randn(100)
    RES_DT = dt.math.floor(dt.Frame(arr))
    RES_NP = numpy.floor(arr)
    assert_equals(RES_DT, dt.Frame(RES_NP))




#-------------------------------------------------------------------------------
# math.fmod()
#-------------------------------------------------------------------------------

def test_fmod():
    DT = dt.Frame([(1.5, 7.7),
                   (3.1, 2.0),
                   (1.0, None),
                   (None, 1.0),
                   (None, None),
                   (0.0, 0.0),
                   (3.4, 0.0),
                   (222.5, math.inf),
                   (-1.1, 3.4)], names=["A", "B"])
    RES = DT[:, dt.math.fmod(f.A, f.B)]
    assert_equals(RES, dt.Frame([1.5, 1.1, None, None, None, None, None,
                                 222.5, -1.1]))


def test_fmod_random(numpy):
    arr1 = numpy.random.randn(100)
    arr2 = numpy.random.randn(100)
    RES_DT = dt.Frame(A=arr1, B=arr2)[:, dt.math.fmod(f.A, f.B)]
    RES_NP = numpy.fmod(arr1, arr2)
    assert_equals(RES_DT, dt.Frame(RES_NP))




#-------------------------------------------------------------------------------
# math.isclose()
#-------------------------------------------------------------------------------

def test_isclose():
    DT = dt.Frame([(1.0, 1.0000001),
                   (1e10, 1.000001e10),
                   (1e-11, 1e-12),
                   (1.1, -1.3),
                   (3e-9, -3e-9),
                   (None, None),
                   (None, 0.0),
                   (0.0, 1e-10)], names=["A", "B"])
    RES_DT = DT[:, dt.math.isclose(f.A, f.B)]
    assert_equals(RES_DT,
                  dt.Frame([True, True, True, False, True, True, False, True]))


def test_isclose_random(numpy):
    arr1 = numpy.random.randn(100)
    arr2 = numpy.random.randn(100)
    DT = dt.Frame(A=arr1, B=arr2)
    RES_DT = DT[:, dt.math.isclose(f.A, f.B, rtol=0.5, atol=0.1)]
    RES_NP = numpy.isclose(arr1, arr2, rtol=0.5, atol=0.1)
    assert_equals(RES_DT, dt.Frame(RES_NP))




#-------------------------------------------------------------------------------
# math.isfinite()
#-------------------------------------------------------------------------------

@pytest.mark.parametrize("src", srcs_int + srcs_float)
def test_dt_isfinite(src):
    DT = dt.Frame(src)
    DT1 = DT[:, dt.math.isfinite(f[0])]
    frame_integrity_check(DT1)
    assert DT1.stypes == (stype.bool8,)
    pyans = [(False if x is None else math.isfinite(x)) for x in src]
    assert DT1.to_list()[0] == pyans


@pytest.mark.parametrize("src", srcs_bool + srcs_int + srcs_float)
def test_dt_isfinite_scalar(src):
    for val in src:
        exp = not (val is None or abs(val) == math.inf)
        assert dt.math.isfinite(val) == exp


def test_dt_isfinite_scalar_wrong_arg():
    with pytest.raises(TypeError, match="Function `isfinite` cannot be applied "
                                        "to a column of type `str32`"):
        dt.math.isfinite("hello")




#-------------------------------------------------------------------------------
# math.isinf()
#-------------------------------------------------------------------------------

def test_isinf():
    assert dt.math.isinf(1) is False
    assert dt.math.isinf(1.5) is False
    assert dt.math.isinf(math.inf) is True
    assert dt.math.isinf(-math.inf) is True
    assert dt.math.isinf(math.nan) is False
    assert dt.math.isinf(False) is False
    assert dt.math.isinf(None) is False


@pytest.mark.parametrize("src", srcs_int + srcs_float)
def test_isinf_srcs(src):
    DT = dt.Frame(src)
    RES = DT[:, dt.math.isinf(f[0])]
    assert_equals(RES, dt.Frame([x == math.inf or x == -math.inf
                                 for x in src]))




#-------------------------------------------------------------------------------
# math.isna()
#-------------------------------------------------------------------------------

@pytest.mark.parametrize("src", srcs_bool + srcs_int + srcs_float + srcs_str)
def test_dt_isna(src):
    DT = dt.Frame(src)
    RES = DT[:, dt.math.isna(f[0])]
    assert_equals(RES, dt.Frame([(x is None) for x in src]))


def test_dt_isna2():
    from math import nan
    DT = dt.Frame(A=[1, None, 2, 5, None, 3.6, nan, -4.899])
    DT1 = DT[~dt.math.isna(f.A), :]
    assert DT1.stypes == DT.stypes
    assert DT1.names == DT.names
    assert DT1.to_list() == [[1.0, 2.0, 5.0, 3.6, -4.899]]


def test_dt_isna_joined():
    # See issue #2109
    DT = dt.Frame(A=[None, 4, 3, 2, 1])
    JDT = dt.Frame(A=[0, 1, 3, 7],
                   B=['a', 'b', 'c', 'd'],
                   C=[0.25, 0.5, 0.75, 1.0],
                   D=[22, 33, 44, 55],
                   E=[True, False, True, False])
    JDT.key = 'A'
    RES = DT[:, dt.math.isna(g[1:]), join(JDT)]
    frame_integrity_check(RES)
    assert RES.to_list() == [[True, True, False, True, False]] * 4


@pytest.mark.parametrize("src", srcs_bool + srcs_int + srcs_float + srcs_str)
def test_dt_math_isna_scalar(src):
    for val in src:
        assert dt.math.isna(val) == (val is None or val is math.nan)



#-------------------------------------------------------------------------------
# math.ldexp()
#-------------------------------------------------------------------------------

def test_ldexp():
    DT = dt.Frame(A=[1.5, 3.5, 100.1, None, 0.0],
                  B=[-1, 14, 5, 9, -5])
    RES = DT[:, dt.math.ldexp(f.A, f.B)]
    assert_equals(RES, dt.Frame([0.75, 57344.0, 3203.2, None, 0.0]))


def test_ldexp_wrong_types():
    DT = dt.Frame(A=[True, False], B=[3.4, 5.6], C=[2, 1])
    with pytest.raises(TypeError):
        assert DT[:, dt.math.ldexp(f.A, f.C)]
    with pytest.raises(TypeError):
        assert DT[:, dt.math.ldexp(f.A, f.B)]
    with pytest.raises(TypeError):
        assert DT[:, dt.math.ldexp(f.B, f.A)]
    with pytest.raises(TypeError):
        assert DT[:, dt.math.ldexp(f.C, f.A)]
    with pytest.raises(TypeError):
        assert DT[:, dt.math.ldexp(f.C, f.B)]




#-------------------------------------------------------------------------------
# math.rint()
#-------------------------------------------------------------------------------

def test_rint():
    assert dt.math.rint(1) == 1.0
    assert isinstance(dt.math.rint(1), float)
    assert dt.math.rint(True) == 1.0
    assert dt.math.rint(3.5) == 4.0
    assert dt.math.rint(3.49) == 3.0
    assert dt.math.rint(3.51) == 4.0
    assert dt.math.rint(1e100) == 1e100
    assert dt.math.rint(None) is None
    assert dt.math.rint(math.nan) is None
    with pytest.raises(TypeError):
        assert dt.math.rint("---")


@pytest.mark.parametrize("src", srcs_bool + srcs_int + srcs_float)
def test_rint_srcs(src):
    DT = dt.Frame(src)
    RES = dt.math.rint(DT)
    assert_equals(RES, dt.Frame([None if x is None else
                                 x if x == math.inf or x == -math.inf else
                                 round(x)
                                 for x in src],
                                stype=dt.float64))


def test_rint_wrong_type():
    msg = "Function `rint` cannot be applied to a column of type `str32`"
    with pytest.raises(TypeError, match=msg):
        DT = dt.Frame(['foo'])
        assert DT[:, dt.math.rint(f.C0)]


def test_rint_random(numpy):
    arr = numpy.random.randn(100)
    RES_DT = dt.math.rint(dt.Frame(arr))
    RES_NP = numpy.rint(arr)
    assert_equals(RES_DT, dt.Frame(RES_NP))




#-------------------------------------------------------------------------------
# math.sign()
#-------------------------------------------------------------------------------

def test_sign():
    assert isinstance(dt.math.sign(1), float)
    assert dt.math.sign(1) == 1.0
    assert dt.math.sign(-1) == -1.0
    assert dt.math.sign(0) == 0.0
    assert dt.math.sign(+0.0) == 0.0
    assert dt.math.sign(-0.0) == 0.0
    assert dt.math.sign(-math.inf) == -1.0
    assert dt.math.sign(+math.inf) == 1.0
    assert dt.math.sign(math.nan) is None
    assert dt.math.sign(None) is None
    assert dt.math.sign(False) == 0.0
    assert dt.math.sign(True) == 1.0


def test_sign_frame():
    DT = dt.Frame(A=[3.3, -5.4, 0.1, 0.0, None],
                  B=[12, 5, -111, None, 0])
    RES = DT[:, dt.math.sign(f[:])]
    assert_equals(RES, dt.Frame(A=[1.0, -1.0, 1.0, 0.0, None],
                                B=[1.0, 1.0, -1.0, None, 0.0]))




#-------------------------------------------------------------------------------
# math.signbit()
#-------------------------------------------------------------------------------

def test_signbit():
    assert isinstance(dt.math.signbit(1), bool)
    assert dt.math.signbit(1) is False
    assert dt.math.signbit(-1) is True
    assert dt.math.signbit(0) is False
    assert dt.math.signbit(+0.0) is False
    assert dt.math.signbit(-0.0) is True
    assert dt.math.signbit(-math.inf) is True
    assert dt.math.signbit(+math.inf) is False
    assert dt.math.signbit(math.nan) is None
    assert dt.math.signbit(None) is None
    assert dt.math.signbit(False) is False
    assert dt.math.signbit(True) is False


def test_signbit_frame():
    DT = dt.Frame(A=[3.3, -5.4, 0.1, 0.0, None],
                  B=[12, 5, -111, None, 0])
    RES = DT[:, dt.math.signbit(f[:])]
    assert_equals(RES, dt.Frame(A=[False, True, False, False, None],
                                B=[False, False, True, None, False]))




#-------------------------------------------------------------------------------
# math.trunc()
#-------------------------------------------------------------------------------

def test_trunc():
    assert dt.math.trunc(1) == 1.0
    assert isinstance(dt.math.trunc(1), float)
    assert dt.math.trunc(True) == 1.0
    assert dt.math.trunc(3.5) == 3.0
    assert dt.math.trunc(3.49) == 3.0
    assert dt.math.trunc(3.51) == 3.0
    assert dt.math.trunc(-3.51) == -3.0
    assert dt.math.trunc(-3.50) == -3.0
    assert dt.math.trunc(-3.49) == -3.0
    assert dt.math.trunc(1e100) == 1e100
    assert dt.math.trunc(None) is None
    assert dt.math.trunc(math.nan) is None
    with pytest.raises(TypeError):
        assert dt.math.trunc("+++")


@pytest.mark.parametrize("src", srcs_bool + srcs_int + srcs_float)
def test_trunc_srcs(src):
    DT = dt.Frame(src)
    RES = dt.math.trunc(DT)
    assert_equals(RES, dt.Frame([None if x is None else
                                 x if x == math.inf or x == -math.inf else
                                 math.trunc(x)
                                 for x in src],
                                stype=dt.float64))


def test_trunc_wrong_type():
    msg = "Function `trunc` cannot be applied to a column of type `str32`"
    with pytest.raises(TypeError, match=msg):
        DT = dt.Frame(['foo'])
        assert DT[:, dt.math.trunc(f.C0)]


def test_trunc_random(numpy):
    arr = numpy.random.randn(100)
    RES_DT = dt.math.trunc(dt.Frame(arr))
    RES_NP = numpy.trunc(arr)
    assert_equals(RES_DT, dt.Frame(RES_NP))
