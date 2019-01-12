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
from datatable import f, ltype, first, count


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
    df_reduce.internal.check()
    assert df_reduce.shape == (1, 2)
    assert df_reduce.ltypes == (ltype.int, ltype.int)
    assert df_reduce.to_list() == [[10], [13]]


def test_count_dt_groupby_integer():
    df_in = dt.Frame([9, 8, 2, 3, None, None, 3, 0, 5, 5, 8, None, 1])
    df_reduce = df_in[:, [count(f.C0), count()], "C0"]
    df_reduce.internal.check()
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
    df_reduce.internal.check()
    assert df_reduce.shape == (1, 3)
    assert df_reduce.ltypes == (ltype.int, ltype.int, ltype.int)
    assert df_reduce.to_list() == [[10], [12], [13]]


def test_count_2d_dt_groupby_integer():
    df_in = dt.Frame([[9, 8, 2, 3, None, None, 3, 0, 5, 5, 8, None, 1],
                      [0, 1, 0, 5, 3, 8, 1, 0, 2, 5, None, 8, 1]])
    df_reduce = df_in[:, [count(f.C0), count(f.C1), count()], "C0"]
    df_reduce.internal.check()
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
    df_reduce.internal.check()
    assert df_reduce.shape == (1, 2)
    assert df_reduce.ltypes == (ltype.int, ltype.int,)
    assert df_reduce.to_list() == [[9], [13]]


def test_count_dt_groupby_string():
    df_in = dt.Frame([None, "blue", "green", "indico", None, None, "orange",
                      "red", "violet", "yellow", "green", None, "blue"])
    df_reduce = df_in[:, [count(f.C0), count()], "C0"]
    df_reduce.internal.check()
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
    df_reduce.internal.check()
    assert df_reduce.shape == (1, 1)
    assert df_reduce.ltypes == (ltype.int,)
    assert df_reduce.to_list() == [[9]]


def test_first_dt_range():
    df_in = dt.Frame(A=range(10))[3::3, :]
    df_reduce = df_in[:, first(f.A)]
    df_reduce.internal.check()
    assert df_reduce.shape == (1, 1)
    assert df_reduce.ltypes == (ltype.int,)
    assert df_reduce.to_list() == [[3]]


def test_first_dt_groupby():
    df_in = dt.Frame([9, 8, 2, 3, None, None, 3, 0, 5, 5, 8, None, 1])
    df_reduce = df_in[:, first(f.C0), "C0"]
    df_reduce.internal.check()
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
    df_reduce.internal.check()
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
