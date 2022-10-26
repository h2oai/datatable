#!/usr/bin/env python
# -*- coding: utf-8 -*-
#-------------------------------------------------------------------------------
# Copyright 2022 H2O.ai
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
from datatable import dt, f, fillna, FExpr, by
from tests import assert_equals


#-------------------------------------------------------------------------------
# Errors
#-------------------------------------------------------------------------------

def test_fillna_reverse_not_a_boolean():
    msg = "Expected a boolean, instead got <class 'str'>"
    DT = dt.Frame([1, 2, None, 4, 5])
    with pytest.raises(TypeError, match=msg):
        DT[:, fillna(f[0], reverse='True')]


def test_fillna_no_argument():
    msg = r"Function datatable.fillna\(\) " \
          "requires exactly 1 positional argument, but none were given"
    with pytest.raises(TypeError, match=msg):
        fillna()


def test_fillna_values_count():
    msg = r"The number of columns in function datatable.fillna\(\) does not " \
          "match the number of the provided values: 2 vs 3"
    DT = dt.Frame([1, 2, None, 4, 5])
    with pytest.raises(TypeError, match=msg):
        DT[:, fillna(f[0, -1], value=[2, 3, 4] )]


def test_fillna_value_reverse():
    msg = r"Parameters value and reverse in function datatable.fillna\(\) " \
          "cannot be both set at the same time"
    DT = dt.Frame([1, 2, None, 4, 5])
    with pytest.raises(ValueError, match=msg):
        DT[:, fillna(f[0, -1], value=2, reverse=False)]



#-------------------------------------------------------------------------------
# Normal
#-------------------------------------------------------------------------------

def test_fillna_str():
  assert str(fillna(f.A)) == "FExpr<" + fillna.__name__ + "(f.A, reverse=False)>"
  assert str(fillna(f.A, reverse=False)) == "FExpr<" + fillna.__name__ + "(f.A, reverse=False)>"
  assert str(fillna(f.A, reverse=False) + 1) == "FExpr<" + fillna.__name__ + "(f.A, reverse=False) + 1>"
  assert str(fillna(f.A + f.B, reverse=True)) == "FExpr<" + fillna.__name__ + "(f.A + f.B, reverse=True)>"
  assert str(fillna(f.B, reverse=True)) == "FExpr<" + fillna.__name__ + "(f.B, reverse=True)>"
  assert str(fillna(f[:2], reverse=False)) == "FExpr<"+ fillna.__name__ + "(f[:2], reverse=False)>"
  assert str(fillna(f.A, value=2)) == "FExpr<" + fillna.__name__ + "(f.A, value=2)>"
  assert str(fillna([f.A, f.B], value=2)) == "FExpr<" + fillna.__name__ + "([f.A, f.B], value=2)>"


def test_fillna_empty_frame():
    DT = dt.Frame()
    fexpr_fillna = fillna(DT, reverse=False)
    assert isinstance(fexpr_fillna, FExpr)
    assert_equals(DT[:, fexpr_fillna], DT)


def test_fillna_empty_frame_value():
    DT = dt.Frame()
    fexpr_fillna = fillna(DT, value=2)
    assert isinstance(fexpr_fillna, FExpr)
    assert_equals(DT[:, fexpr_fillna], DT)


def test_fillna_void():
    DT = dt.Frame([None, None, None])
    DT_fillna = DT[:, fillna(f[:], reverse=True)]
    assert_equals(DT_fillna, DT)


def test_fillna_void_value():
    DT = dt.Frame([None, None, None])
    DT_fillna = DT[:, fillna(f[:], value=2)]
    DT_ref = dt.Frame([2, 2, 2]/dt.int32)
    assert_equals(DT_fillna, DT_ref)


def test_fillna_trivial():
    DT = dt.Frame([1, None])
    DT_fillna = DT[:, fillna(f[:])]
    DT_ref = dt.Frame([1, 1])
    assert_equals(DT_fillna, DT_ref)


def test_fillna_trivial_value():
    DT = dt.Frame([None]/dt.int64)
    DT_fillna = DT[:, fillna(f[:], value=1)]
    DT_ref = dt.Frame([1]/dt.int64)
    assert_equals(DT_fillna, DT_ref)


def test_fillna_bool():
    DT = dt.Frame([None, False, None, True, False, True])
    DT_fillna = DT[:, [fillna(f[:], reverse=False),
                       fillna(f[:], reverse=True)]]
    DT_ref = dt.Frame([
                [None, False, False, True, False, True],
                [False, False, True, True, False, True]
             ])
    assert_equals(DT_fillna, DT_ref)


def test_fillna_bool_value():
    DT = dt.Frame([None, False, None, True, False, True])
    DT_fillna = DT[:, [fillna(f[:], value=2),
                       fillna(f[:], value=2.0)]]
    DT_ref = dt.Frame([
                [2, 0, 2, 1, 0, 1]/dt.int32,
                [2, 0, 2, 1, 0, 1]/dt.float64,
             ])
    assert_equals(DT_fillna, DT_ref)


def test_fillna_int():
    DT = dt.Frame([None, None, 3, None, 4, -100, None])
    DT_fillna = DT[:, [fillna(f[:], reverse=False),
                       fillna(f[:], reverse=True)]]
    DT_ref = dt.Frame([
                  [None, None, 3, 3, 4, -100, -100],
                  [3, 3, 3, 4, 4, -100, None]
              ])
    assert_equals(DT_fillna, DT_ref)


def test_fillna_int_with_float_value():
    DT = dt.Frame([None, None, 3, None, 4, -100, None])
    DT_fillna = DT[:, fillna(f[:], value=-3.1415)]
    DT_ref = dt.Frame([-3.1415, -3.1415, 3, -3.1415, 4, -100, -3.1415])
    assert_equals(DT_fillna, DT_ref)


def test_fillna_string():
    DT = dt.Frame([None, 'a', None, 'b', None])
    DT_fillna = DT[:, [fillna(f[:]), fillna(f[:], reverse=True)]]
    DT_ref = dt.Frame([
                [None, 'a', 'a', 'b', 'b'],
                ['a', 'a', 'b', 'b', None]
             ])
    assert_equals(DT_fillna, DT_ref)


def test_fillna_string_value():
    DT = dt.Frame([None, 'a', None, 'b'])
    DT_fillna = DT[:, fillna(f[:], value='None')]
    DT_ref = dt.Frame([['None', 'a', 'None', 'b']])
    assert_equals(DT_fillna, DT_ref)


def test_fillna_multicolumn_value():
    DT = dt.Frame([[15, None, 136, 93, 743, None, None, 91],
                  ['a', None, 'a', None, 'b', 'c', 'c', None]])
    DT_fillna = DT[:, fillna(f[:], value=[-100, 'NNone'])]
    DT_ref = dt.Frame([[15, -100, 136, 93, 743, -100, -100, 91],
                      ['a', 'NNone', 'a', 'NNone', 'b', 'c', 'c', 'NNone']])
    assert_equals(DT_fillna, DT_ref)


def test_fillna_grouped():
    DT = dt.Frame([[15, None, 136, 93, 743, None, None, 91],
                  ['a', 'a', 'a', 'b', 'b', 'c', 'c', 'c']])
    DT_fillna = DT[:, [fillna(f[:]), fillna(f[:], reverse=True)], by(f[-1])]
    DT_ref = dt.Frame({
                'C1':['a','a','a','b','b','c','c','c'],
                'C0':[15, 15, 136, 93, 743, None, None, 91],
                'C2':[15, 136, 136, 93, 743, 91, 91, 91],
             })
    assert_equals(DT_fillna, DT_ref)


def test_fillna_grouped_value():
    DT = dt.Frame([[15, None, 136, 93, 743, None, None, 91],
                  ['a','a','a','b','b','c','c','c']])
    DT_fillna = DT[:, fillna(f[:], value=f.C0.mean()), by(f[-1])]
    DT_ref = dt.Frame({
                'C1' : ['a','a','a','b','b','c','c','c'],
                'C0' : [15, 75.5, 136, 93, 743, 91, 91, 91],
             })
    assert_equals(DT_fillna, DT_ref)


def test_fillna_grouped_value_vs_ifelse():
    DT = dt.Frame([[15, None, 136, 93, 743, None, None, 91],
                  ['a', 'a', 'a', 'b', 'b', 'c', 'c', 'c']])
    DT_fillna = DT[:, fillna(f.C0, value=f.C0.mean()), by(f[-1])]
    DT_ref = DT[:, dt.ifelse(f.C0 == None, f.C0.mean(), f.C0).alias('C0'), f.C1]
    assert_equals(DT_fillna, DT_ref)


def test_fillna_grouped_column():
    DT = dt.Frame([2, 1, None, 1, 2])
    DT_fillna = DT[:, [fillna(f[0], reverse=False),
                      fillna(f[0], reverse=True)], by(f[0])]
    DT_ref = dt.Frame([
                 [None, 1, 1, 2, 2],
                 [None, 1, 1, 2, 2],
                 [None, 1, 1, 2, 2]
             ])
    assert_equals(DT_fillna, DT_ref)


def test_fillna_groupby_complex():
    DT = dt.Frame([[3, None, None, 15, 92, 6],
                  ["a", "a", "cat", "a", "dog", "cat"]])
    DT_fillna = DT[:, fillna(f[0], value=f[0].min()), by(f[1])]

    DT_ref = dt.Frame({
               "C1" : ["a", "a", "a", "cat", "cat", "dog"],
               "C0" : [3, 3, 15, 6, 6, 92]
             })
    assert_equals(DT_fillna, DT_ref)

