#!/usr/bin/env python
# -*- coding: utf-8 -*-
#-------------------------------------------------------------------------------
# Copyright 2018 H2O.ai
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
import datatable as dt
import math
import pytest
from datatable import f, by, ltype, first, count, median
from datatable.internal import frame_integrity_check
from tests import noop


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
    a_in = [9, 8, 2, 3, None, None, 3, 0, 5, 5, 8, None, 1]
    a_reduce = first(a_in)
    assert a_reduce == 9


def test_first_dt():
    df_in = dt.Frame([9, 8, 2, 3, None, None, 3, 0, 5, 5, 8, None, 1])
    df_reduce = df_in[:, first(f.C0)]
    frame_integrity_check(df_reduce)
    assert df_reduce.shape == (1, 1)
    assert df_reduce.ltypes == (ltype.int,)
    assert df_reduce.to_list() == [[9]]


def test_first_dt_range():
    df_in = dt.Frame(A=range(10))[3::3, :]
    df_reduce = df_in[:, first(f.A)]
    frame_integrity_check(df_reduce)
    assert df_reduce.shape == (1, 1)
    assert df_reduce.ltypes == (ltype.int,)
    assert df_reduce.to_list() == [[3]]


def test_first_dt_groupby():
    df_in = dt.Frame([9, 8, 2, 3, None, None, 3, 0, 5, 5, 8, None, 1])
    df_reduce = df_in[:, first(f.C0), "C0"]
    frame_integrity_check(df_reduce)
    assert df_reduce.shape == (8, 2)
    assert df_reduce.ltypes == (ltype.int, ltype.int,)
    assert df_reduce.to_list() == [[None, 0, 1, 2, 3, 5, 8, 9],
                                   [None, 0, 1, 2, 3, 5, 8, 9]]


def test_first_dt_integer_large(numpy):
    n = 12345678
    a_in = numpy.random.randint(2**20, size=n, dtype=numpy.int32)
    df_in = dt.Frame(a_in)
    df_reduce = df_in[:, first(f.C0)]
    assert df_reduce.shape == (1, 1)
    assert df_reduce.ltypes == (ltype.int,)
    assert df_reduce.to_list() == [[a_in[0]]]


def test_first_2d_dt():
    df_in = dt.Frame([[9, 8, 2, 3, None, None, 3, 0, 5, 5, 8, None, 1],
                      [0, 1, 0, 5, 3, 8, 1, 0, 2, 5, 8, None, 1]])
    df_reduce = df_in[:, [first(f.C0), first(f.C1)], "C0"]
    frame_integrity_check(df_reduce)
    assert df_reduce.shape == (8, 3)
    assert df_reduce.ltypes == (ltype.int, ltype.int, ltype.int,)
    assert df_reduce.to_list() == [[None, 0, 1, 2, 3, 5, 8, 9],
                                   [None, 0, 1, 2, 3, 5, 8, 9],
                                   [3, 0, 1, 0, 5, 2, 1, 0]]


def test_first_2d_array():
    a_in = [[9, 8, 2, 3, None, None, 3, 0, 5, 5, 8, None, 1],
            [0, 1, 0, 5, 3, 8, 1, 0, 2, 5, 8, None, 1]]
    a_reduce = first(a_in)
    assert a_reduce == [9, 8, 2, 3, None, None, 3, 0, 5, 5, 8, None, 1]




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
