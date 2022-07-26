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
from datatable import dt, f, cumcount, ngroup, FExpr, by
from tests import assert_equals


#-------------------------------------------------------------------------------
# Errors
#-------------------------------------------------------------------------------

def test_cumcount_non_bool():
    DT = dt.Frame(list('abcde'))
    msg = r'Argument reverse in function datatable.cumcount\(\) should be a boolean.+'
    with pytest.raises(TypeError, match = msg):
        DT[:, cumcount('False')]

def test_ngroup_non_bool():
    DT = dt.Frame(list('abcde'))
    msg = r'Argument reverse in function datatable.ngroup\(\) should be a boolean.+'
    with pytest.raises(TypeError, match = msg):
        DT[:, ngroup('True'), by(f[0])]


#-------------------------------------------------------------------------------
# Normal
#-------------------------------------------------------------------------------

def test_cumcount_ngroup_str():
  assert str(cumcount(False)) == "FExpr<cumcount(reverse=False)>"
  assert str(cumcount(True)) == "FExpr<cumcount(reverse=True)>"
  assert str(ngroup(False)) == "FExpr<ngroup(reverse=False)>"
  assert str(ngroup(True)) == "FExpr<ngroup(reverse=True)>"
  assert str(cumcount(False) + 1) == "FExpr<cumcount(reverse=False) + 1>"
  assert str(ngroup(False) + 1) == "FExpr<ngroup(reverse=False) + 1>"


def test_cumcount_ngroup_empty_frame():
    DT = dt.Frame()
    assert isinstance(cumcount(False), FExpr)
    assert isinstance(ngroup(True), FExpr)
    assert_equals(DT[:, cumcount(False)], dt.Frame({'C0': []/dt.int64}))
    assert_equals(DT[:, ngroup(True)], dt.Frame({'C0': []/dt.int64}))


def test_cumcount_ngroup_void():
    DT = dt.Frame([None]*10)
    DT = DT[:, [cumcount(True), cumcount(False), ngroup(True)]]
    DT_ref = dt.Frame([list(range(9, -1, -1))/dt.int64,
                       list(range(10))/dt.int64,
                       [0]*10/dt.int64])
    assert_equals(DT, DT_ref)


def test_cumcount_ngroup_trivial():
    DT = dt.Frame([0]/dt.int64)
    DT = DT[:, [cumcount(True), cumcount(False), ngroup(True), ngroup(False)]]
    DT_ref = dt.Frame([[0]/dt.int64, [0]/dt.int64, [0]/dt.int64, [0]/dt.int64])
    assert_equals(DT, DT_ref)


def test_cumcount_ngroup_small():
    DT = dt.Frame(['a', 'a','a','b','b','a'])
    DT = DT[:, [cumcount(False), cumcount(True), ngroup(True), ngroup(False)]]
    DT_ref = dt.Frame([list(range(6))/dt.int64,
                      list(range(5, -1, -1))/dt.int64,
                      [0]*6/dt.int64,
                      [0]*6/dt.int64])
    assert_equals(DT, DT_ref)


def test_cumcount_ngroup_groupby():
    DT = dt.Frame(['a', 'a', 'a', 'b', 'b', 'a'])
    DT = DT[:, [cumcount(False), ngroup(True)], by(f[0])]
    DT_ref = dt.Frame([['a', 'a', 'a', 'a', 'b', 'b'],
                      [0, 1, 2, 3, 0, 1]/dt.int64,
                      [1, 1, 1, 1, 0, 0]/dt.int64])
    assert_equals(DT, DT_ref)
