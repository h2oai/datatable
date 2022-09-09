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

msg =  "Argument reverse in function datatable.fillna\\(\\) should be a boolean, "
msg += "instead got <class 'str'>"
def test_fillna_reverse_not_a_boolean():
    DT = dt.Frame([1, 2, None, 4, 5])
    with pytest.raises(TypeError, match = msg):
        DT[:, fillna(f[0], reverse='True')]


def test_fillna_reverse_not_a_boolean_by():
    DT = dt.Frame([1, 2, None, 4, 5])
    with pytest.raises(TypeError, match = msg):
        DT[:, fillna(f[0], reverse = 'True'), by(f[0])]


def test_fillna_no_argument():
    msg = (f"Function datatable.fillna\\(\\) "
            "requires at least 1 positional argument, but none were given")
    with pytest.raises(TypeError, match = msg):
        fillna()


#-------------------------------------------------------------------------------
# Normal
#-------------------------------------------------------------------------------

def test_fillna_str():
  assert str(fillna(f.A, reverse=False)) == "FExpr<" + fillna.__name__ + "(f.A, reverse=False)>"
  assert str(fillna(f.A, reverse=False) + 1) == "FExpr<" + fillna.__name__ + "(f.A, reverse=False) + 1>"
  assert str(fillna(f.A + f.B, reverse = True)) == "FExpr<" + fillna.__name__ + "(f.A + f.B, reverse=True)>"
  assert str(fillna(f.B, reverse = True)) == "FExpr<" + fillna.__name__ + "(f.B, reverse=True)>"
  assert str(fillna(f[:2], reverse = False)) == "FExpr<"+ fillna.__name__ + "(f[:2], reverse=False)>"


def test_fillna_empty_frame():
    DT = dt.Frame()
    expr_fillna = fillna(DT, reverse=False)
    assert isinstance(expr_fillna, FExpr)
    assert_equals(DT[:, fillna(f[:], reverse=False)], DT)


def test_fillna_void():
    DT = dt.Frame([None, None, None])
    DT_fillna = DT[:, fillna(f[:], reverse=True)]
    assert_equals(DT_fillna, DT)


def test_fillna_trivial():
    DT = dt.Frame([0]/dt.int64)
    fillna_fexpr = fillna(f[:], reverse = True)
    DT_fillna = DT[:, fillna_fexpr]
    assert isinstance(fillna_fexpr, FExpr)
    assert_equals(DT, DT_fillna)


def test_fillna_bool():
    DT = dt.Frame([None, False, None, True, False, True])
    DT_fillna = DT[:, [fillna(f[:], reverse = False),
                       fillna(f[:], reverse = True)]]
    DT_ref = dt.Frame([
                [None, False, False, True, False, True],
                [False, False, True, True, False, True]
             ])
    assert_equals(DT_fillna, DT_ref)


def test_fillna_small():
    DT = dt.Frame([None, 3, None, 4])
    DT_fillna = DT[:, [fillna(f[:], reverse = False),
                       fillna(f[:], reverse = True)]]
    DT_ref = dt.Frame([
                  [None, 3, 3, 4],
                  [3, 3, 4, 4]
              ])
    assert_equals(DT_fillna, DT_ref)


def test_fillna_string():
    DT = dt.Frame([None, 'a', None, 'b'])
    DT_fillna = DT[:, [fillna(f[:]), fillna(f[:], reverse = True)]]
    DT_ref = dt.Frame([
                [None, 'a', 'a', 'b'],
                ['a', 'a', 'b', 'b']
             ])
    assert_equals(DT_fillna, DT_ref)


def test_fillna_grouped():
    DT = dt.Frame([[15, None, 136, 93, 743, None, None, 91],
                  ['a','a','a','b','b','c','c','c']])
    DT_fillna = DT[:, [fillna(f[:]), fillna(f[:], reverse = True)], by(f[-1])]
    DT_ref = dt.Frame({
                'C1':['a','a','a','b','b','c','c','c'],
                'C0':[15, 15, 136, 93, 743, None, None, 91],
                'C2':[15, 136, 136, 93, 743, 91, 91, 91],
             })
    assert_equals(DT_fillna, DT_ref)


def test_fillna_grouped_column():
    DT = dt.Frame([2, 1, None, 1, 2])
    DT_fillna = DT[:, [fillna(f[0], reverse = False),
                      fillna(f[0], reverse = True)], by(f[0])]
    DT_ref = dt.Frame([
                 [None, 1, 1, 2, 2],
                 [None, 1, 1, 2, 2],
                 [None, 1, 1, 2, 2]
             ])
    assert_equals(DT_fillna, DT_ref)


def test_fillna_groupby_complex():
    DT = dt.Frame([[3, None, 15, 92, 6], ["a", "cat", "a", "dog", "cat"]])
    DT_fillna = DT[:, fillna(f[0].min()), by(f[1])]

    DT_ref = dt.Frame({
               "C1" : ["a", "a", "cat", "cat", "dog"],
               "C0" : [3, 3, 6, 6, 92]
             })
    assert_equals(DT_fillna, DT_ref)

