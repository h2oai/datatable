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
from tests import list_equals, assert_equals, noop


# Sets of tuples containing test columns of each type
srcs_bool = [[False, True, False, False, True],
             [True, None, None, True, False]]

srcs_int = [[5, -3, 6, 3, 0],
            [None, -1, 0, 26, -3],
            # TODO: currently ~ operation fails on v = 2**31 - 1. Should we
            #       promote the resulting column to int64 in such case?
            [2**31 - 2, -(2**31 - 1), 0, -1, 1]]

srcs_float = [[9.5, 0.2, 5.4857301, -3.14159265338979],
              [1.1, 2.3e12, -.5, None, float("inf"), 0.0]]

srcs_str = [["foo", "bbar", "baz"],
            [None, "", " ", "  ", None, "\x00"],
            list("qwertyuiiop[]asdfghjkl;'zxcvbnm,./`1234567890-=")]



#-------------------------------------------------------------------------------
# Unary bitwise NOT (__invert__)
#-------------------------------------------------------------------------------

def inv(t):
    if t is None: return None
    if isinstance(t, bool): return not t
    return ~t


def test_dt_invert():
    srcs = srcs_bool + srcs_int
    DT = dt.Frame(srcs)
    RES = DT[:, [~f[i] for i in range(DT.ncols)]]
    frame_integrity_check(RES)
    assert RES.stypes == DT.stypes
    assert RES.to_list() == [[inv(x) for x in src] for src in srcs]


@pytest.mark.parametrize("src", srcs_float + srcs_str)
def test_dt_invert_invalid(src):
    DT = dt.Frame(src)
    col_stype = DT.stype.name
    with pytest.raises(TypeError, match="Cannot apply unary `operator ~` to a "
                                        "column with stype `%s`" % col_stype):
        assert DT[:, ~f[0]]




#-------------------------------------------------------------------------------
# Unary minus (__neg__)
#-------------------------------------------------------------------------------

def neg(t):
    if t is None: return None
    return -t


@pytest.mark.parametrize("src", srcs_int + srcs_float + srcs_bool)
def test_dt_neg(src):
    DT = dt.Frame(src)
    RES = DT[:, -f[0]]
    frame_integrity_check(RES)
    stype0 = dt.int32 if DT.stype in [dt.bool8, dt.int8, dt.int16] else \
             DT.stype
    assert RES.stype == stype0
    assert_equals(RES, dt.Frame([neg(x) for x in src], stype=RES.stype))


@pytest.mark.parametrize("src", srcs_str)
def test_dt_neg_invalid(src):
    DT = dt.Frame(src)
    col_stype = DT.stype.name
    with pytest.raises(TypeError, match="Cannot apply unary `operator -` to a "
                                        "column with stype `%s`" % col_stype):
        assert DT[:, -f[0]]




#-------------------------------------------------------------------------------
# Unary plus (__pos__)
#-------------------------------------------------------------------------------

@pytest.mark.parametrize("src", srcs_int + srcs_float + srcs_bool)
def test_dt_pos(src):
    DT = dt.Frame(src)
    dtr = DT[:, +f[0]]
    frame_integrity_check(dtr)
    stype0 = DT.stype
    if stype0 in [stype.bool8, stype.int8, stype.int16]:
        stype0 = stype.int32
    assert dtr.stype == stype0
    assert list_equals(dtr.to_list()[0], list(src))


@pytest.mark.parametrize("src", srcs_str)
def test_dt_pos_invalid(src):
    DT = dt.Frame(src)
    col_stype = DT.stype.name
    with pytest.raises(TypeError, match="Cannot apply unary `operator \\+` to "
                                        "a column with stype `%s`" % col_stype):
        assert DT[:, +f[0]]




#-------------------------------------------------------------------------------
# isna()
#-------------------------------------------------------------------------------

@pytest.mark.parametrize("src", srcs_bool + srcs_int + srcs_float + srcs_str)
def test_dt_isna(src):
    DT = dt.Frame(src)
    DT1 = DT[:, dt.math.isna(f[0])]
    frame_integrity_check(DT1)
    assert DT1.stypes == (stype.bool8,)
    pyans = [x is None for x in src]
    assert DT1.to_list()[0] == pyans


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


@pytest.mark.parametrize("src", srcs_int + srcs_float)
def test_dt_math_isna_scalar_not_implemented(src):
    for val in src:
        with pytest.raises(NotImplementedError,
                           match = "This operation is not implemented yet"):
            dt.math.isna(val)


@pytest.mark.parametrize("src", [srcs_bool[0], srcs_str[0]])
def test_dt_math_isna_scalar_wrong_arg(src):
    for val in src:
        with pytest.raises(TypeError,
                           match = r"Function `isna\(\)` cannot be applied "
                           "to an argument of type " + str(type(val))):
            dt.math.isna(val)


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


@pytest.mark.parametrize("src", srcs_int + srcs_float)
def test_dt_isfinite_scalar_not_implemented(src):
    for val in src:
        with pytest.raises(NotImplementedError,
                           match = "This operation is not implemented yet"):
            dt.math.isfinite(val)


@pytest.mark.parametrize("src", [srcs_bool[0], srcs_str[0]])
def test_dt_isfinite_scalar_wrong_arg(src):
    for val in src:
        with pytest.raises(TypeError,
                           match = r"Function `isfinite\(\)` cannot be applied "
                           "to an argument of type " + str(type(val))):
            dt.math.isfinite(val)


#-------------------------------------------------------------------------------
# abs()
#-------------------------------------------------------------------------------

def test_abs():
    from datatable import abs
    assert abs(1) == 1
    assert abs(-5) == 5
    assert abs(-2.5e12) == 2.5e12
    assert abs(None) is None


@pytest.mark.parametrize("src", srcs_int + srcs_float)
def test_abs_srcs(src):
    DT = dt.Frame(src)
    DT1 = dt.abs(DT)
    frame_integrity_check(DT1)
    assert DT.stypes == DT1.stypes
    pyans = [None if x is None else abs(x) for x in src]
    assert DT1.to_list()[0] == pyans


def test_abs_all_stypes():
    from datatable import abs
    src = [[-127, -5, -1, 0, 2, 127],
           [-32767, -299, -7, 32767, 12, -543],
           [-2147483647, -1000, 3, -589, 2147483647, 0],
           [-2**63 + 1, 2**63 - 1, 0, -2**32, 2**32, -793]]
    DT = dt.Frame(src, stypes=[dt.int8, dt.int16, dt.int32, dt.int64])
    DT1 = DT[:, [abs(f[i]) for i in range(4)]]
    frame_integrity_check(DT1)
    assert DT1.to_list() == [[abs(x) for x in col] for col in src]




#-------------------------------------------------------------------------------
# exp()
#-------------------------------------------------------------------------------

def test_exp():
    from datatable import exp
    assert exp(0) == math.exp(0)
    assert exp(1) == math.exp(1)
    assert exp(-2.5e12) == math.exp(-2.5e12)
    assert exp(12345678) == math.inf
    assert exp(None) is None


@pytest.mark.parametrize("src", srcs_int + srcs_float)
def test_exp_srcs(src):
    from math import exp, inf
    DT = dt.Frame(src)
    DT1 = dt.exp(DT)
    frame_integrity_check(DT1)
    assert all([st == stype.float64 for st in DT1.stypes])
    pyans = []
    for x in src:
        if x is None:
            pyans.append(None)
        else:
            try:
                pyans.append(exp(x))
            except OverflowError:
                pyans.append(inf)
    assert DT1.to_list()[0] == pyans


def test_exp_all_stypes():
    from datatable import exp
    src = [[-127, -5, -1, 0, 2, 127],
           [-32767, -299, -7, 32767, 12, -543],
           [-2147483647, -1000, 3, -589, 2147483647, 0],
           [-2 ** 63 + 1, 2 ** 63 - 1, 0, -2 ** 32, 2 ** 32, -793]]
    DT = dt.Frame(src, stypes=[dt.int8, dt.int16, dt.int32, dt.int64])
    DT1 = DT[:, [exp(f[i]) for i in range(4)]]
    frame_integrity_check(DT1)
    pyans = []
    for col in src:
        l = []
        for x in col:
            if x is None:
                l.append(None)
            else:
                try:
                    l.append(math.exp(x))
                except OverflowError:
                    l.append(math.inf)
        pyans.append(l)
    assert DT1.to_list() == pyans




#-------------------------------------------------------------------------------
# log(), log10()
#-------------------------------------------------------------------------------

@pytest.mark.parametrize("fn", ["log", "log10"])
def test_log(fn):
    dtlog = getattr(dt, fn)
    mathlog = getattr(math, fn)
    assert dtlog(None) is None
    assert dtlog(-1) is None
    assert dtlog(0) == -math.inf
    assert dtlog(1) == 0.0
    assert dtlog(5) == mathlog(5)
    assert dtlog(10) == mathlog(10)
    assert dtlog(793.54) == mathlog(793.54)
    assert dtlog(math.inf) == math.inf


@pytest.mark.parametrize("src", srcs_int + srcs_float)
@pytest.mark.parametrize("fn", ["log", "log10"])
def test_log_srcs(src, fn):
    dtlog = getattr(dt, fn)
    mathlog = getattr(math, fn)
    DT = dt.Frame(src)
    DT1 = dtlog(DT)
    frame_integrity_check(DT1)
    assert all([st == stype.float64 for st in DT1.stypes])
    pyans = [None if x is None or x < 0 else
             -math.inf if x == 0 else
             mathlog(x)
             for x in src]
    assert DT1.to_list()[0] == pyans



#-------------------------------------------------------------------------------
# len()
#-------------------------------------------------------------------------------

def test_len():
    DT = dt.Frame(A=["", "one", "2", "three", "four", None, "six", "seventy"])
    RES = DT[:, f.A.len()]
    assert RES.stype == dt.stype.int64
    assert RES.to_list() == [[0, 3, 1, 5, 4, None, 3, 7]]


def test_len2():
    DT = dt.Frame([None, "", "mooo" * 10000], stype="str64")
    RES = DT[:, f[0].len()]
    assert RES.stype == dt.stype.int64
    assert RES.to_list() == [[None, 0, 40000]]


def test_len_wrong_col():
    DT = dt.Frame(range(34))
    with pytest.raises(TypeError, match=r"Cannot apply function `len\(\)` to a "
                                        r"column with stype `int32`"):
        assert DT[:, f[0].len()]


def test_len_unicode():
    DT = dt.Frame(["майдан", "蒙蒂巨蟒", "🤥", "𝔘𝔫𝔦𝔠𝔬𝔡𝔢"])
    RES = DT[:, f[0].len()]
    assert_equals(RES, dt.Frame([6, 4, 1, 7], stype=dt.int64))
