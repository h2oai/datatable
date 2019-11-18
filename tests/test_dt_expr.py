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
import math
import pytest
import random
import datatable as dt
from datatable import f, stype, ltype
from datatable.internal import frame_integrity_check
from tests import list_equals, assert_equals, noop


# Sets of tuples containing test columns of each type
dt_bool = [[False, True, False, False, True],
           [True, None, None, True, False]]

dt_int = [[5, -3, 6, 3, 0],
          [None, -1, 0, 26, -3],
          # TODO: currently ~ operation fails on v = 2**31 - 1. Should we
          #       promote the resulting column to int64 in such case?
          [2**31 - 2, -(2**31 - 1), 0, -1, 1]]

dt_float = [[9.5, 0.2, 5.4857301, -3.14159265338979],
            [1.1, 2.3e12, -.5, None, float("inf"), 0.0]]

dt_str = [["foo", "bbar", "baz"],
          [None, "", " ", "  ", None, "\x00"],
          list("qwertyuiiop[]asdfghjkl;'zxcvbnm,./`1234567890-=")]

dt_obj = [[dt, pytest, random, f, dt_int, None],
          ["one", None, 3, 9.99, stype.int32, object, isinstance]]



#-------------------------------------------------------------------------------
# logical ops
#-------------------------------------------------------------------------------

def test_logical_and1():
    src1 = [1, 5, 12, 3, 7, 14]
    src2 = [1, 2] * 3
    ans = [i for i in range(6)
           if src1[i] < 10 and src2[i] == 1]

    df0 = dt.Frame(A=src1, B=src2)
    df1 = df0[(f.A < 10) & (f.B == 1), [f.A, f.B]]
    assert df1.to_list() == [[src1[i] for i in ans],
                             [src2[i] for i in ans]]


def test_logical_or1():
    src1 = [1, 5, 12, 3, 7, 14]
    src2 = [1, 2] * 3
    ans = [i for i in range(6)
           if src1[i] < 10 or src2[i] == 1]

    df0 = dt.Frame(A=src1, B=src2)
    df1 = df0[(f.A < 10) | (f.B == 1), [f.A, f.B]]
    assert df1.to_list() == [[src1[i] for i in ans],
                             [src2[i] for i in ans]]


@pytest.mark.parametrize("seed", [random.getrandbits(63)])
def test_logical_and2(seed):
    random.seed(seed)
    n = 1000
    src1 = [random.choice([True, False, None]) for _ in range(n)]
    src2 = [random.choice([True, False, None]) for _ in range(n)]

    df0 = dt.Frame(A=src1, B=src2)
    df1 = df0[:, f.A & f.B]
    assert df1.to_list()[0] == \
        [False if (src1[i] is False or src2[i] is False) else
         None if (src1[i] is None or src2[i] is None) else
         True
         for i in range(n)]


@pytest.mark.parametrize("seed", [random.getrandbits(63)])
def test_logical_or2(seed):
    random.seed(seed)
    n = 1000
    src1 = [random.choice([True, False, None]) for _ in range(n)]
    src2 = [random.choice([True, False, None]) for _ in range(n)]

    df0 = dt.Frame(A=src1, B=src2)
    df1 = df0[:, f.A | f.B]
    assert df1.to_list()[0] == \
        [True if (src1[i] is True or src2[i] is True) else
         None if (src1[i] is None or src2[i] is None) else
         False
         for i in range(n)]



#-------------------------------------------------------------------------------
# Equality operators
#-------------------------------------------------------------------------------

@pytest.mark.parametrize("st1, st2", [(dt.str32, dt.str32),
                                      (dt.str32, dt.str64),
                                      (dt.str64, dt.str32),
                                      (dt.str64, dt.str64)])
def test_equal_strings(st1, st2):
    df0 = dt.Frame([["foo", "gowebrt", None, "afdw", "req",  None],
                    ["foo", "goqer",   None, "afdw", "req?", ""]],
                   names=["A", "B"], stypes=[st1, st2])
    assert df0.stypes == (st1, st2)
    df1 = df0[:, f.A == f.B]
    assert df1.stypes == (dt.bool8,)
    assert df1.to_list() == [[True, False, True, True, False, False]]
    df2 = df0[:, f.A != f.B]
    assert df2.stypes == (dt.bool8,)
    assert df2.to_list() == [[False, True, False, False, True, True]]


def test_equal_columnset():
    DT = dt.Frame([[1, 2, 3], [5, 4, 1]], names=["A", "B"])
    DT2 = DT[:, f[:] == 1]
    DT3 = DT[:, 2 < f[:]]
    assert_equals(DT2, dt.Frame([[True, False, False], [False, False, True]]))
    assert_equals(DT3, dt.Frame([[False, False, True], [True, True, False]]))



#-------------------------------------------------------------------------------
# Division
#-------------------------------------------------------------------------------

@pytest.mark.parametrize("seed", [random.getrandbits(63)])
def test_div_mod(seed):
    random.seed(seed)
    n = 1000
    src1 = [random.randint(-100, 100) for _ in range(n)]
    src2 = [random.randint(-10, 10) for _ in range(n)]

    df0 = dt.Frame(x=src1, y=src2)
    df1 = df0[:, [f.x // f.y, f.x % f.y, f.x / f.y]]
    assert df1.to_list() == [
        [None if src2[i] == 0 else src1[i] // src2[i] for i in range(n)],
        [None if src2[i] == 0 else src1[i] % src2[i] for i in range(n)],
        [None if src2[i] == 0 else src1[i] / src2[i] for i in range(n)]
    ]


def test_issue1562():
    DT = dt.Frame(A=[-8], B=[1677])
    assert DT[:, f.A / f.B][0, 0] == -(8.0 / 1677.0)



#-------------------------------------------------------------------------------
# Misc
#-------------------------------------------------------------------------------

def test_expr_reuse():
    # Check that an expression can later be used in another selector
    # In particular, we first apply `expr` to `df0` (where A is the second
    # column). If `expr` were to memorize that "A" maps to the second column,
    # it would produce an error when applied to the second Frame which has
    # only 1 column. See #1366.
    expr = f.A < 1
    df0 = dt.Frame([range(5), range(5)], names=["B", "A"])
    df1 = df0[:, {"A": expr}]
    frame_integrity_check(df1)
    assert df1.names == ("A", )
    assert df1.stypes == (stype.bool8, )
    assert df1.to_list() == [[True, False, False, False, False]]
    df2 = df1[:, expr]
    frame_integrity_check(df2)
    assert df2.to_list() == [[False, True, True, True, True]]


def test_expr_names():
    # See issue #1963
    DT = dt.Frame(A=range(5), B=range(5))
    assert DT[:, [f.A, f.A + f.B]].names == ("A", "C0")
