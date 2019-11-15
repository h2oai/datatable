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
from datatable import (
    dt, f, by, ltype, first, last, count, median, sum, mean, cov, corr)
from datatable.internal import frame_integrity_check
from tests import assert_equals, noop


#-------------------------------------------------------------------------------
# Count
#-------------------------------------------------------------------------------

def test_count_array_integer():
    a_in = [9, 8, 2, 3, None, None, 3, 0, 5, 5, 8, None, 1]
    a_reduce = count(a_in)
    assert a_reduce == 10


def test_count_dt_integer():
    df_in = dt.Frame([9, 8, 2, 3, None, None, 3, 0, 5, 5, 8, None, 1])
    df_reduce = df_in[:, [count(f.C0), count()]]
    frame_integrity_check(df_reduce)
    assert df_reduce.shape == (1, 2)
    assert df_reduce.ltypes == (ltype.int, ltype.int)
    assert df_reduce.to_list() == [[10], [13]]


def test_count_dt_groupby_integer():
    df_in = dt.Frame([9, 8, 2, 3, None, None, 3, 0, 5, 5, 8, None, 1])
    df_reduce = df_in[:, [count(f.C0), count()], "C0"]
    frame_integrity_check(df_reduce)
    assert df_reduce.shape == (8, 3)
    assert df_reduce.ltypes == (ltype.int, ltype.int, ltype.int,)
    assert df_reduce.to_list() == [[None, 0, 1, 2, 3, 5, 8, 9],
                                   [0, 1, 1, 1, 2, 2, 2, 1],
                                   [3, 1, 1, 1, 2, 2, 2, 1]]


def test_count_2d_array_integer():
    a_in = [[9, 8, 2, 3, None, None, 3, 0, 5, 5, 8, None, 1],
            [0, 1, 0, 5, 3, 8, 1, 0, 2, 5, None, 8, 1]]
    a_reduce = count(a_in)
    assert a_reduce == 2


def test_count_2d_dt_integer():
    df_in = dt.Frame([[9, 8, 2, 3, None, None, 3, 0, 5, 5, 8, None, 1],
                      [0, 1, 0, 5, 3, 8, 1, 0, 2, 5, None, 8, 1]])
    df_reduce = df_in[:, [count(f.C0), count(f.C1), count()]]
    frame_integrity_check(df_reduce)
    assert df_reduce.shape == (1, 3)
    assert df_reduce.ltypes == (ltype.int, ltype.int, ltype.int)
    assert df_reduce.to_list() == [[10], [12], [13]]


def test_count_2d_dt_groupby_integer():
    df_in = dt.Frame([[9, 8, 2, 3, None, None, 3, 0, 5, 5, 8, None, 1],
                      [0, 1, 0, 5, 3, 8, 1, 0, 2, 5, None, 8, 1]])
    df_reduce = df_in[:, [count(f.C0), count(f.C1), count()], "C0"]
    frame_integrity_check(df_reduce)
    assert df_reduce.shape == (8, 4)
    assert df_reduce.ltypes == (ltype.int,) * 4
    assert df_reduce.to_list() == [[None, 0, 1, 2, 3, 5, 8, 9],
                                   [0, 1, 1, 1, 2, 2, 2, 1],
                                   [3, 1, 1, 1, 2, 2, 1, 1],
                                   [3, 1, 1, 1, 2, 2, 2, 1]]


def test_count_array_string():
    a_in = [None, "blue", "green", "indico", None, None, "orange", "red",
            "violet", "yellow", "green", None, "blue"]
    a_reduce = count(a_in)
    assert a_reduce == 9


def test_count_dt_string():
    df_in = dt.Frame([None, "blue", "green", "indico", None, None, "orange",
                      "red", "violet", "yellow", "green", None, "blue"])
    df_reduce = df_in[:, [count(f.C0), count()]]
    frame_integrity_check(df_reduce)
    assert df_reduce.shape == (1, 2)
    assert df_reduce.ltypes == (ltype.int, ltype.int,)
    assert df_reduce.to_list() == [[9], [13]]


def test_count_dt_groupby_string():
    df_in = dt.Frame([None, "blue", "green", "indico", None, None, "orange",
                      "red", "violet", "yellow", "green", None, "blue"])
    df_reduce = df_in[:, [count(f.C0), count()], "C0"]
    frame_integrity_check(df_reduce)
    assert df_reduce.shape == (8, 3)
    assert df_reduce.ltypes == (ltype.str, ltype.int, ltype.int,)
    assert df_reduce.to_list() == [[None, "blue", "green", "indico", "orange",
                                    "red", "violet", "yellow"],
                                   [0, 2, 2, 1, 1, 1, 1, 1],
                                   [4, 2, 2, 1, 1, 1, 1, 1]]


def test_count_dt_integer_large(numpy):
    n = 12345678
    a_in = numpy.random.randint(2**20, size=n, dtype=numpy.int32)
    df_in = dt.Frame(a_in)
    df_reduce = df_in[:, count()]
    assert df_reduce.shape == (1, 1)
    assert df_reduce.ltypes == (ltype.int,)
    assert df_reduce.to_list() == [[n]]


def test_count_with_i():
    # See issue 1316
    DT = dt.Frame(A=range(100))
    assert DT[:5, count()][0, 0] == 5
    assert DT[-12:, count()][0, 0] == 12
    assert DT[::3, count()][0, 0] == 34




#-------------------------------------------------------------------------------
# First
#-------------------------------------------------------------------------------

def test_first_array():
    assert first([9, 8, 2, 3, None, None, 3, 0, 5, 5, 8, None, 1]) == 9
    assert first((3.5, 17.9, -4.4)) == 3.5
    assert first([]) == None


def test_first_dt():
    df_in = dt.Frame([9, 8, 2, 3, None, None, 3, 0, 5, 5, 8, None, 1])
    df_reduce = df_in[:, first(f.C0)]
    assert_equals(df_reduce, dt.Frame(C0=[9]))


def test_first_empty_frame():
    DT = dt.Frame(A=[], stype=dt.float32)
    RZ = DT[:, first(f.A)]
    assert_equals(RZ, dt.Frame(A=[None], stype=dt.float32))


def test_first_dt_range():
    df_in = dt.Frame(A=range(10))[3::3, :]
    df_reduce = df_in[:, first(f.A)]
    assert_equals(df_reduce, dt.Frame(A=[3]))


def test_first_dt_groupby():
    df_in = dt.Frame([9, 8, 2, 3, None, None, 3, 0, 5, 5, 8, None, 1])
    df_reduce = df_in[:, first(f.C0), "C0"]
    assert_equals(df_reduce, dt.Frame([[None, 0, 1, 2, 3, 5, 8, 9],
                                       [None, 0, 1, 2, 3, 5, 8, 9]],
                                      names=["C0", "C1"]))


def test_first_dt_integer_large(numpy):
    n = 12345678
    a_in = numpy.random.randint(2**20, size=n, dtype=numpy.int32)
    df_in = dt.Frame(a_in)
    df_reduce = df_in[:, first(f.C0)]
    assert_equals(df_reduce, dt.Frame(C0=[a_in[0]]))


def test_first_2d_dt():
    df_in = dt.Frame([[9, 8, 2, 3, None, None, 3, 0, 5, 5, 8, None, 1],
                      [0, 1, 0, 5, 3, 8, 1, 0, 2, 5, 8, None, 1]])
    df_reduce = df_in[:, [first(f.C0), first(f.C1)], "C0"]
    assert_equals(df_reduce, dt.Frame([[None, 0, 1, 2, 3, 5, 8, 9],
                                       [None, 0, 1, 2, 3, 5, 8, 9],
                                       [3, 0, 1, 0, 5, 2, 1, 0]],
                                      names=["C0", "C1", "C2"]))


def test_first_2d_array():
    a_in = [[9, 8, 2, 3, None, None, 3, 0, 5, 5, 8, None, 1],
            [0, 1, 0, 5, 3, 8, 1, 0, 2, 5, 8, None, 1]]
    a_reduce = first(a_in)
    assert a_reduce == [9, 8, 2, 3, None, None, 3, 0, 5, 5, 8, None, 1]



#-------------------------------------------------------------------------------
# Last
#-------------------------------------------------------------------------------

def test_last_array():
    assert last([1, 5, 7]) == 7
    assert last("dlvksjdnf") == "f"
    assert last(x.upper() for x in "abcd") == "D"
    assert last(x * 2 for x in "") == None
    assert last([]) == None

def test_last_frame():
    DT = dt.Frame(A=[1, 3, 7], B=[None, "er", "hooray"])
    RZ = DT[:, last(f[:])]
    assert_equals(RZ, DT[-1, :])


def test_last_empty_frame():
    DT = dt.Frame(A=[], B=[], C=[], stypes=[dt.float32, dt.bool8, dt.str64])
    RZ = DT[:, last(f[:])]
    DT.nrows = 1
    assert_equals(RZ, DT)



#-------------------------------------------------------------------------------
# min/max
#-------------------------------------------------------------------------------

@pytest.mark.parametrize("mm", [dt.min, dt.max])
@pytest.mark.parametrize("st", dt.ltype.int.stypes)
def test_minmax_integer(mm, st):
    src = [0, 23, 100, 99, -11, 24, -1]
    DT = dt.Frame(A=src, stype=st)
    assert DT[:, mm(f.A)].to_list() == [[mm(src)]]


@pytest.mark.parametrize("mm", [dt.min, dt.max])
def test_minmax_real(mm):
    src = [5.6, 12.99, 1e+12, -3.4e-22, math.nan, 0.0]
    DT = dt.Frame(A=src)
    assert DT[:, mm(f.A)].to_list() == [[mm(src)]]


@pytest.mark.parametrize("mm", [dt.min, dt.max])
def test_minmax_infs(mm):
    src = [math.nan, 1.0, 2.5, -math.inf, 3e199, math.inf]
    answer = -math.inf if mm == dt.min else +math.inf
    DT = dt.Frame(A=src)
    assert DT[:, mm(f.A)].to_list() == [[answer]]


@pytest.mark.parametrize("mm", [dt.min, dt.max])
@pytest.mark.parametrize("st", dt.ltype.int.stypes + dt.ltype.real.stypes)
def test_minmax_empty(mm, st):
    DT1 = dt.Frame(A=[], stype=st)
    assert DT1[:, mm(f.A)].to_list() == [[None]]


@pytest.mark.parametrize("mm", [dt.min, dt.max])
@pytest.mark.parametrize("st", dt.ltype.int.stypes + dt.ltype.real.stypes)
def test_minmax_nas(mm, st):
    DT2 = dt.Frame(B=[None]*3, stype=st)
    assert DT2[:, mm(f.B)].to_list() == [[None]]




#-------------------------------------------------------------------------------
# sum
#-------------------------------------------------------------------------------

def test_sum_simple():
    DT = dt.Frame(A=range(5))
    R = DT[:, sum(f.A)]
    frame_integrity_check(R)
    assert R.to_list() == [[10]]
    assert str(R)


def test_sum_empty_frame():
    DT = dt.Frame([[]] * 4, names=list("ABCD"),
                  stypes=(dt.bool8, dt.int32, dt.float32, dt.float64))
    assert DT.shape == (0, 4)
    RZ = DT[:, sum(f[:])]
    frame_integrity_check(RZ)
    assert RZ.shape == (1, 4)
    assert RZ.names == ("A", "B", "C", "D")
    assert RZ.stypes == (dt.int64, dt.int64, dt.float32, dt.float64)
    assert RZ.to_list() == [[0], [0], [0], [0]]
    assert str(RZ)




#-------------------------------------------------------------------------------
# Mean
#-------------------------------------------------------------------------------

def test_mean_simple():
    DT = dt.Frame(A=range(5))
    RZ = DT[:, mean(f.A)]
    frame_integrity_check(RZ)
    assert RZ.stypes == (dt.float64,)
    assert RZ.to_list() == [[2.0]]


def test_mean_empty_frame():
    DT = dt.Frame([[]] * 4, names=list("ABCD"),
                  stypes=(dt.bool8, dt.int32, dt.float32, dt.float64))
    assert DT.shape == (0, 4)
    RZ = DT[:, mean(f[:])]
    frame_integrity_check(RZ)
    assert RZ.shape == (1, 4)
    assert RZ.names == ("A", "B", "C", "D")
    assert RZ.stypes == (dt.float64, dt.float64, dt.float32, dt.float64)
    assert RZ.to_list() == [[None]] * 4





#-------------------------------------------------------------------------------
# Median
#-------------------------------------------------------------------------------

def test_median_empty_frame():
    DT = dt.Frame(A=[])
    RES = DT[:, median(f.A)]
    assert RES.shape == (1, 1)
    assert RES.to_list() == [[None]]


def test_median_bool_even_nrows():
    DT = dt.Frame(A=[True, False, True, False])
    RES = DT[:, median(f.A)]
    assert RES.shape == (1, 1)
    assert RES.stypes == (dt.float64,)
    assert RES[0, 0] == 0.5


def test_median_bool_odd_nrows():
    DT2 = dt.Frame(B=[True, False, True])
    RES2 = DT2[:, median(f.B)]
    assert RES2.shape == (1, 1)
    assert RES2.stypes == (dt.float64,)
    assert RES2[0, 0] == 1.0


def test_median_bygroup():
    DT = dt.Frame(A=[0.1, 0.2, 0.5, 0.4, 0.3, 0], B=[1, 2, 1, 1, 2, 2])
    RZ = DT[:, median(f.A), by(f.B)]
    # group 1: 0.1, 0.4, 0.5
    # group 2: 0.0, 0.2, 0.3
    assert RZ.to_list() == [[1, 2], [0.4, 0.2]]


@pytest.mark.parametrize("st", dt.ltype.int.stypes)
def test_median_int_even_nrows(st):
    # data points in the middle: 5 and 7
    DT = dt.Frame(A=[7, 11, -2, 3, 0, 12, 12, 3, 5, 91], stype=st)
    RES = DT[:, median(f.A)]
    assert RES.shape == (1, 1)
    assert RES.stypes == (dt.float64,)
    assert RES[0, 0] == 6.0


@pytest.mark.parametrize("st", dt.ltype.int.stypes)
def test_median_int_odd_nrows(st):
    # data points in the middle: 5 and 7
    DT = dt.Frame(A=[4, -5, 12, 11, 4, 7, 0, 23, 45, 8, 10], stype=st)
    RES = DT[:, median(f.A)]
    assert RES.shape == (1, 1)
    assert RES.stypes == (dt.float64,)
    assert RES[0, 0] == 8.0


def test_median_int_no_overflow():
    # If median calculation done inaccurately, 111+112 may overflow int8,
    # giving a negative result
    DT = dt.Frame(A=[111, 112], stype=dt.int8)
    RES = DT[:, median(f.A)]
    assert RES[0, 0] == 111.5


@pytest.mark.parametrize("st", [dt.float32, dt.float64])
def test_median_float(st):
    DT = dt.Frame(W=[0.0, 5.5, 7.9, math.inf, -math.inf], stype=st)
    RES = DT[:, median(f.W)]
    assert RES.shape == (1, 1)
    assert RES.stypes == (st,)
    assert RES[0, 0] == 5.5  # 5.5 has same value in float64 and float32


def test_median_all_nas():
    DT = dt.Frame(N=[math.nan] * 8)
    RES = DT[:, median(f.N)]
    assert RES.shape == (1, 1)
    assert RES.stypes == (dt.float64,)
    assert RES[0, 0] is None


def test_median_some_nas():
    DT = dt.Frame(S=[None, 5, None, 12, None, -3, None, None, None, 4])
    RES = DT[:, median(f.S)]
    assert RES.shape == (1, 1)
    assert RES.stypes == (dt.float64,)
    assert RES[0, 0] == 4.5


def test_median_grouped():
    DT = dt.Frame(A=[0, 0, 0, 0, 1, 1, 1, 1, 1],
                  B=[2, 6, 1, 0, -3, 4, None, None, -1],
                  stypes={"A": dt.int16, "B": dt.int32})
    RES = DT[:, median(f.B), by(f.A)]
    assert RES.shape == (2, 2)
    assert RES.stypes == (dt.int16, dt.float64)
    assert RES.to_list() == [[0, 1], [1.5, -1.0]]


def test_median_wrong_stype():
    DT = dt.Frame(A=["foo"], B=["moo"], stypes={"A": dt.str32, "B": dt.str64})
    with pytest.raises(TypeError) as e:
        noop(DT[:, median(f.A)])
    assert ("Unable to apply reduce function `median()` to a column of "
            "type `str32`" in str(e.value))
    with pytest.raises(TypeError) as e:
        noop(DT[:, median(f.B)])
    assert ("Unable to apply reduce function `median()` to a column of "
            "type `str64`" in str(e.value))



#-------------------------------------------------------------------------------
# Cov
#-------------------------------------------------------------------------------

def test_cov_simple():
    DT = dt.Frame(A=range(5), B=range(5))
    D1 = DT[:, cov(f.A, f.B)]
    assert_equals(D1, dt.Frame([2.5]))


def test_cov_small_frame():
    D1 = dt.Frame(A=[1], B=[2])[:, cov(f.A, f.B)]
    D2 = dt.Frame(A=[], B=[])[:, cov(f.A, f.B)]
    assert_equals(D1, dt.Frame([None], stype=dt.float64))
    assert_equals(D2, dt.Frame([None], stype=dt.float64))


def test_cov_subframe():
    DT = dt.Frame(A=range(100))
    D1 = DT[37:40, cov(f.A, f.A)]
    assert D1[0, 0] == 1.0


def test_cov_float32():
    DT = dt.Frame(A=[1.0, 2.0, 3.0], B=[7.5, 7.0, 6.5], stype=dt.float32)
    assert DT.stype == dt.float32
    D1 = DT[:, cov(f.A, f.B)]
    assert_equals(D1, dt.Frame([-0.5], stype=dt.float32))


def test_cov_bygroup():
    DT = dt.Frame(ID=[1, 2, 1, 2, 1, 2], A=[0, 5, 10, 20, 2, 8])
    D1 = DT[:, cov(f.A, f.A), by(f.ID)]
    assert_equals(D1, dt.Frame(ID=[1, 2], C0=[28.0, 63.0]))


@pytest.mark.parametrize("seed", [random.getrandbits(32)])
def test_cov_random(numpy, seed):
    numpy.random.seed(seed)
    arr1 = numpy.random.rand(100)
    arr2 = numpy.random.rand(100)
    np_cov = numpy.cov(arr1, arr2)[0, 1]

    DT = dt.Frame([arr1, arr2])
    dt_cov = DT[:, cov(f[0], f[1])][0, 0]
    assert numpy.isclose(np_cov, dt_cov, atol=0, rtol=1e-12)




#-------------------------------------------------------------------------------
# Corr
#-------------------------------------------------------------------------------

def test_corr_simple():
    DT = dt.Frame(A=range(5), B=range(5))
    D1 = DT[:, corr(f.A, f.B)]
    assert_equals(D1, dt.Frame([1.0]))


def test_corr_simple2():
    DT = dt.Frame(A=range(5), B=range(5, 0, -1))
    D1 = DT[:, corr(f.A, f.B)]
    assert_equals(D1, dt.Frame([-1.0]))


def test_corr_small_frame():
    D1 = dt.Frame(A=[1], B=[2])[:, corr(f.A, f.B)]
    D2 = dt.Frame(A=[], B=[])[:, corr(f.A, f.B)]
    assert_equals(D1, dt.Frame([None], stype=dt.float64))
    assert_equals(D2, dt.Frame([None], stype=dt.float64))


def test_corr_with_constant():
    DT = dt.Frame(A=range(23), B=[2.5] * 23)
    D1 = DT[:, corr(f.A, f.B)]
    assert_equals(D1, dt.Frame([math.nan]))


@pytest.mark.parametrize("seed", [random.getrandbits(32)])
def test_corr_random(numpy, seed):
    numpy.random.seed(seed)
    arr1 = numpy.random.rand(100)
    arr2 = numpy.random.rand(100)
    np_corr = numpy.corrcoef(arr1, arr2)[0, 1]

    DT = dt.Frame([arr1, arr2])
    dt_corr = DT[:, corr(f[0], f[1])][0, 0]
    assert numpy.isclose(np_corr, dt_corr, atol=0, rtol=1e-12)


def test_corr_multiple():
    DT = dt.Frame(A=[3, 5, 9, 1], B=[4, 7, 0, 0], C=[3, 2, 1, 0], D=range(4))
    D1 = DT[:, corr(f.A, f[:])]
    D2 = DT[:, corr(f[:], f.D)]
    D3 = DT[:, corr(f[:], f[:])]
    a = -0.07168504827326534
    b = 0.07559289460184544
    c = 0.7207110797203374
    assert_equals(D1, dt.Frame([[1.0], [a], [b], [-b]]))
    assert_equals(D2, dt.Frame([[-b], [-c], [-1.0], [1.0]]))
    assert_equals(D3, dt.Frame([[1.0], [1.0], [1.0], [1.0]]))
