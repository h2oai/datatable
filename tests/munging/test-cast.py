#!/usr/bin/env python
# -*- coding: utf-8 -*-
#-------------------------------------------------------------------------------
# Copyright 2018-2023 H2O.ai
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
#
# Note: we do not test the cast behavior to integer during overflow, as it can
#       be implementation-dependent.
#
#-------------------------------------------------------------------------------
import math
import pytest
import datatable as dt
from datatable import f, stype, ltype, as_type
from datatable.internal import frame_columns_virtual, frame_integrity_check
from tests import noop, assert_equals


numeric_stypes = ltype.bool.stypes + ltype.int.stypes + ltype.real.stypes
all_stypes = numeric_stypes + ltype.str.stypes + ltype.obj.stypes

valid_stype_pairs = []
for source_stype in all_stypes:
    for target_stype in all_stypes:
        if (target_stype in numeric_stypes and
            source_stype not in numeric_stypes): continue
        valid_stype_pairs.append((source_stype, target_stype))



#-------------------------------------------------------------------------------
# Cast to int
#-------------------------------------------------------------------------------

@pytest.mark.parametrize("target_stype", ltype.int.stypes)
def test_cast_bool_to_int(target_stype):
    DT = dt.Frame(B=[None, True, None, False, True, False, True])
    assert DT.stypes == (dt.bool8,)
    RES = DT[:, target_stype(f.B)]
    assert RES.stypes == (target_stype,)
    assert RES.to_list()[0] == [None, 1, None, 0, 1, 0, 1]


@pytest.mark.parametrize("source_stype", ltype.int.stypes + ltype.real.stypes)
@pytest.mark.parametrize("target_stype", ltype.int.stypes)
def test_cast_numeric_to_int(source_stype, target_stype):
    DT = dt.Frame(N=[-3, -1, None, 0, 1, 3], stype=source_stype)
    assert DT.stypes == (source_stype,)
    assert DT.names == ("N",)
    RES = DT[:, target_stype(f.N)]
    assert RES.stypes == (target_stype,)
    assert RES.to_list()[0] == [-3, -1, None, 0, 1, 3]


@pytest.mark.parametrize("target_stype", ltype.int.stypes)
def test_cast_large_to_int(target_stype):
    # Check that integers close to the max for `target_stype` do not
    # get clobbered.
    m = target_stype.max
    DT = dt.Frame(Q=[-m, -m + 1, -m//2, 0, m//2, m - 1, m, None],
                  stype=dt.int64)
    RES = DT[:, target_stype(f.Q)]
    assert RES.stypes == (target_stype,)
    assert RES.to_list() == DT.to_list()


@pytest.mark.parametrize("source_stype", ltype.real.stypes)
@pytest.mark.parametrize("target_stype", ltype.int.stypes)
def test_cast_float_to_int(source_stype, target_stype):
    DT = dt.Frame(F=[0.1, math.nan, 7.500, -4.8, 1.111111e2, 28.99999],
                  stype=source_stype)
    assert DT.stypes == (source_stype,)
    RES = DT[:, target_stype(f.F)]
    assert RES.stypes == (target_stype,)
    assert RES.to_list()[0] == [0, None, 7, -4, 111, 28]


def test_cast_str_to_int_simple():
    DT = dt.Frame(A=[str(i) for i in range(100001)])
    DT["A"] = dt.int32
    assert_equals(DT, dt.Frame(A=range(100001)))


@pytest.mark.parametrize("source_stype", ltype.str.stypes)
@pytest.mark.parametrize("target_stype", ltype.int.stypes)
def test_cast_str_to_int(source_stype, target_stype):
    DT = dt.Frame(W=["0", "23", "+56", "-17", "101"], stype=source_stype)
    assert DT.stype == source_stype
    RES = DT[:, target_stype(f.W)]
    assert_equals(RES, dt.Frame(W=[0, 23, 56, -17, 101] / target_stype))


def test_cast_str_zeroes_to_int():
    DT = dt.Frame(["0", "+0", "-0", "00", "+00", "-00", "-000", "0"*100])
    RES = DT[:, dt.int32(f[0])]
    assert_equals(RES, dt.Frame([0] * 8 / dt.int32))


def test_cast_badstr_to_int():
    DT = dt.Frame(["345", "10000000000", "24e100", "abc500", None, "--5",
                   "-", "+", "", "~"])
    RES = DT[:, dt.int32(f[0])]
    assert_equals(RES, dt.Frame([345, 1410065408, None, None, None, None,
                                 None, None, None, None]))


@pytest.mark.parametrize("target_stype", ltype.int.stypes)
def test_cast_str_to_int_with_overflow(target_stype):
    offset = target_stype.max + 1
    mask = offset * 2
    data = [7**i for i in range(1000)]
    expected = [(x + offset) % mask - offset for x in data]
    DT = dt.Frame([str(x) for x in data])
    DT[:, 0] = target_stype
    assert_equals(DT, dt.Frame(expected / target_stype))


@pytest.mark.parametrize("target_stype", ltype.int.stypes + ltype.real.stypes)
def test_cast_object_to_numeric(target_stype):
    DT = dt.Frame(W=[0, 1, 2], stype=stype.obj64)
    assert DT.stypes == (stype.obj64,)
    RES = DT[:, target_stype(f.W)]
    assert_equals(RES, dt.Frame(W=[0, 1, 2], stype=target_stype))


@pytest.mark.parametrize("target_stype", ltype.int.stypes + ltype.real.stypes)
def test_cast_date32_to_numeric(target_stype):
    from datetime import date as d
    DT = dt.Frame([d(1970,1,10), d(1969, 12, 30)])
    assert DT.types == [dt.Type.date32]
    RES = DT[:, target_stype(f[0])]
    assert_equals(RES, dt.Frame([9, -2], stype=target_stype))



#-------------------------------------------------------------------------------
# Cast to float
#-------------------------------------------------------------------------------

@pytest.mark.parametrize("target_stype", ltype.real.stypes)
def test_cast_bool_to_float(target_stype):
    DT = dt.Frame(B=[None, True, None, False, True, False, True, None])
    assert DT.stypes == (dt.bool8,)
    RES = DT[:, target_stype(f.B)]
    assert RES.stypes == (target_stype,)
    assert RES.to_list()[0] == [None, 1.0, None, 0.0, 1.0, 0.0, 1.0, None]


@pytest.mark.parametrize("source_stype", ltype.int.stypes + ltype.real.stypes)
@pytest.mark.parametrize("target_stype", ltype.real.stypes)
def test_cast_numeric_to_float(source_stype, target_stype):
    DT = dt.Frame(E=[-3, -1, None, 0, 1, 3], stype=source_stype)
    assert DT.stypes == (source_stype,)
    assert DT.names == ("E",)
    RES = DT[:, target_stype(f.E)]
    assert RES.stypes == (target_stype,)
    assert RES.to_list()[0] == [-3.0, -1.0, None, 0.0, 1.0, 3.0]


def test_cast_double_to_float():
    DT = dt.Frame(F=[0.78, math.nan, math.inf, -math.inf, 2.35e78, -6.5e11])
    assert DT.stypes == (dt.float64,)
    RES = DT[:, dt.float32(f.F)]
    assert RES.stypes == (dt.float32,)
    assert RES.to_list()[0] == [0.7799999713897705, None, math.inf, -math.inf,
                                math.inf, -6.50000007168e+11]


@pytest.mark.parametrize("target_stype", ltype.real.stypes)
def test_cast_str_to_double(target_stype):
    DT = dt.Frame(A=["2.45", "-3.333", "0.13e+29", "boo", None, "-4e-4", "3.14e+323"])
    DT["A"] = target_stype
    assert_equals(DT, dt.Frame(A=[2.45, -3.333, 1.3e28, None, None, -0.0004, 3.14e+323],
                               stype=target_stype))



#-------------------------------------------------------------------------------
# Cast to str
#-------------------------------------------------------------------------------

def test_cast_bool_to_str():
    DT = dt.Frame(P=[False, None, True, True, False, True])
    assert DT.stypes == (dt.bool8,)
    RES = DT[:, [dt.str32(f.P), dt.str64(f.P)]]
    assert RES.stypes == (dt.str32, dt.str64)
    ans = ["False", None, "True", "True", "False", "True"]
    assert RES.to_list() == [ans, ans]


@pytest.mark.parametrize("source_stype", ltype.int.stypes)
def test_cast_int_to_str(source_stype):
    DT = dt.Frame([None, 0, -3, 189, 77, 14, None, 394831, -52939047130424957],
                  stype=source_stype)
    assert DT.stypes == (source_stype,)
    RES = DT[:, [dt.str32(f.C0), dt.str64(f.C0)]]
    frame_integrity_check(RES)
    assert RES.stypes == (dt.str32, dt.str64)
    assert RES.shape == (DT.nrows, 2)
    ans = [None if v is None else str(v)
           for v in DT.to_list()[0]]
    assert RES.to_list()[0] == ans


@pytest.mark.parametrize("source_stype", ltype.real.stypes)
def test_cast_float_to_str(source_stype):
    DT = dt.Frame(J=[3.5, 7.049, -3.18, math.inf, math.nan, 1.0, -math.inf,
                     1e16, 0],
                  stype=source_stype)
    assert DT.stypes == (source_stype,)
    RES = DT[:, [dt.str32(f.J), dt.str64(f.J)]]
    frame_integrity_check(RES)
    ans = ["3.5", "7.049", "-3.18", "inf", None, "1.0", "-inf", "1.0e+16", "0.0"]
    assert RES.to_list() == [ans, ans]


@pytest.mark.parametrize("source_stype", ltype.str.stypes)
def test_cast_str_to_str(source_stype):
    DT = dt.Frame(S=["Right", "middle", None, "", "A" * 1000],
                  stype=source_stype)
    assert DT.stypes == (source_stype,)
    RES = DT[:, [dt.str32(f.S), dt.str64(f.S)]]
    frame_integrity_check(RES)
    assert RES.to_list() == DT.to_list() * 2


def test_cast_obj_to_str():
    src = [noop, "Hello!", ..., {}, dt, print, None]
    DT = dt.Frame(src, stype=object)
    assert DT.stypes == (dt.obj64,)
    RES = DT[:, [dt.str32(f[0]), dt.str64(f[0])]]
    frame_integrity_check(RES)
    ans = [str(x) for x in src]
    ans[-1] = None
    assert RES.to_list() == [ans, ans]


def test_cast_huge_to_str():
    # Test that converting a huge column into str would properly overflow it
    # into str64 type. See issue #1695
    # This test takes up to 8s to run
    n = 300000000
    DT = dt.Frame(BIG=range(n))
    RES = DT[:, dt.str32(f.BIG)]
    assert RES.stype == dt.str32
    assert RES[-1, 0] == str(n - 1)
    assert RES[3194870, 0] == "3194870"
    RES.materialize()
    assert RES.stype == dt.str64
    assert RES[-1, 0] == str(n - 1)


def test_cast_empty_str32_to_str64():
    # See issue #2369
    DT = dt.Frame(A=[], stype=dt.str32)
    DT['A'] = dt.str64
    assert_equals(DT, dt.Frame(A=[], stype=dt.str64))
    DT['A'] = dt.str32
    assert_equals(DT, dt.Frame(A=[], stype=dt.str32))


@pytest.mark.parametrize("target_stype", [dt.str32, dt.str64])
def test_cast_date32_to_str(target_stype):
    from datetime import date as d
    DT = dt.Frame(A=[d(2000, 1, 5), d(2001, 6, 2), None, d(831, 11, 11)])
    assert DT.types == [dt.Type.date32]
    DT['A'] = target_stype
    assert_equals(DT,
        dt.Frame(A=["2000-01-05", "2001-06-02", None, "0831-11-11"],
                 stype=target_stype))



#-------------------------------------------------------------------------------
# Cast to obj
#-------------------------------------------------------------------------------

def test_cast_to_obj():
    src = [[True, False, None, True, False],
           [1, 7, 15, -1, None],
           [45, None, 1028, -999, 14],
           [29487, -35389, 0, 34, None],
           [None, 3901734019, 2**63 - 1, 1, -1],
           [1.0, None, math.inf, -0.5, 17001],
           [34.50659476, -math.inf, 5.78e+211, -4.19238e-297, None],
           ["Unknown", "secret", "hidden", "*********", None],
           ["Sectumsempra", "Flippendo", "Obliviate", "Accio", "Portus"]]
    stypes = (dt.bool8, dt.int8, dt.int16, dt.int32, dt.int64, dt.float32,
              dt.float64, dt.str32, dt.str64)
    DT = dt.Frame(src, stypes=stypes)
    assert DT.stypes == stypes
    assert DT.to_list() == src
    RES = DT[:, [dt.obj64(f[i]) for i in range(9)]]
    assert RES.stypes == (dt.obj64,) * 9
    assert RES.to_list() == src


def test_cast_obj_to_obj():
    class AA: pass

    a = AA()
    b = AA()
    DT = dt.Frame([a, b, AA, {"believe": "in something"}],
                  stype=dt.obj64)
    assert DT.stypes == (dt.obj64,)
    RES = DT[:, dt.obj64(f[0])]
    assert RES.stypes == (dt.obj64,)
    ans = RES.to_list()[0]
    assert ans[0] is a
    assert ans[1] is b
    assert ans[2] is AA
    assert ans[3] == {"believe": "in something"}


def test_cast_date32_to_obj():
    from datetime import date as d
    src = [d(2000, 1, 5), d(2001, 6, 2), None, d(831, 11, 11)]
    DT = dt.Frame(A=src)
    assert DT.types == [dt.Type.date32]
    DT['A'] = object
    assert_equals(DT, dt.Frame(A=src, stype=dt.obj64))



#-------------------------------------------------------------------------------
# Cast views
#-------------------------------------------------------------------------------

def test_cast_view_simple():
    df0 = dt.Frame({"A": [1, 2, 3]})
    df1 = df0[::-1, :][:, dt.float32(f.A)]
    frame_integrity_check(df1)
    assert df1.stypes == (dt.float32,)
    assert df1.to_list() == [[3.0, 2.0, 1.0]]


@pytest.mark.parametrize("viewtype", [0, 1, 2, 3])
@pytest.mark.parametrize("source_stype, target_stype", valid_stype_pairs)
def test_cast_views_all(viewtype, source_stype, target_stype):
    # TODO: add rowindex with NAs after #1496
    # TODO: add rowindex ARR64 somehow...
    selector = [
        slice(1, None, 2),
        slice(3, -1),
        [5, 2, 3, 0, 0, 3, 7],
        dt.Frame([3, 0, 1, 4], stype=dt.int64)
    ][viewtype]
    DT = dt.Frame(A=range(10), stype=source_stype)
    DT = DT[selector, :]
    assert frame_columns_virtual(DT)[0]
    assert DT.stypes == (source_stype,)

    RES = DT[:, target_stype(f.A)]
    assert RES.stypes == (target_stype,)
    ans1 = RES.to_list()[0]

    DT.materialize()
    ans2 = DT[:, target_stype(f.A)].to_list()[0]
    assert ans1 == ans2
