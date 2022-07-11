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
import math
import pytest
from datatable import dt, f, cumcount, ngroup, FExpr, by
from tests import assert_equals


#-------------------------------------------------------------------------------
# Errors
#-------------------------------------------------------------------------------

def test_cumcount_non_bool():
    DT = dt.Frame(list('abcde'))
    with pytest.raises(TypeError, match = r'Argument reverse in function datatable.cumcount\(\) should be a boolean.+'):
        DT[:, cumcount('False')]

def test_ngroup_non_bool():
    DT = dt.Frame(list('abcde'))
    with pytest.raises(TypeError, match = r'Argument reverse in function datatable.ngroup\(\) should be a boolean.+'):
        DT[:, ngroup('True')]

#-------------------------------------------------------------------------------
# Normal
#-------------------------------------------------------------------------------

def test_cumcount_ngroup_str():
  assert str(cumcount(False)) == "FExpr<cumcount(arg=False)>"
  assert str(cumcount(True)) == "FExpr<cumcount(arg=True)>"
  assert str(ngroup(False)) == "FExpr<ngroup(arg=False)>"
  assert str(ngroup(True)) == "FExpr<ngroup(arg=True)>"
  assert str(cumcount(False) + 1) == "FExpr<cumcount(arg=False) + 1>"
  assert str(ngroup(False) + 1) == "FExpr<ngroup(arg=False) + 1>"



def test_cumcount_ngroup_empty_frame():
    DT = dt.Frame()
    assert isinstance(cumcount(False), FExpr)
    assert isinstance(ngroup(False), FExpr)
    assert_equals(DT[:, cumcount(False)], dt.Frame({'C0': []/dt.int64}))
    assert_equals(DT[:, ngroup(False)], dt.Frame({'C0': []/dt.int64}))


def test_cumcount_ngroup_void():
    DT = dt.Frame([None]*10)
    DT_cnt_ngrp = DT[:, [cumcount(True), ngroup(False)]]
    assert_equals(DT_cnt_ngrp, dt.Frame([list(range(9,-1,-1))/dt.int64, [0]*10/dt.int64]))


def test_cumcount_ngroup_trivial():
    DT = dt.Frame([0]/dt.int64)
    DT_cnt_ngrp = DT[:, [cumcount(True), ngroup(False)]]
    assert_equals(DT_cnt_ngrp, dt.Frame([[0]/dt.int64, [0]/dt.int64]))


def test_cumcount_ngroup_small():
    DT = dt.Frame(['a', 'a','a','b','b','a'])
    DT_cnt_ngrp = DT[:, [cumcount(False), ngroup(True)]]
    assert_equals(DT_cnt_ngrp, dt.Frame([list(range(6))/dt.int64, [0]*6/dt.int64]))


def test_cumcount_ngroup_groupby():
    DT = dt.Frame(['a', 'a','a','b','b','a'])
    DT_cnt_ngrp = DT[:, [cumcount(False), ngroup(True)], by(f[0])]
    DT_ref = dt.Frame([['a', 'a','a','a', 'b','b'],[0,1,2,3,0,1]/dt.int64, [1,1,1,1,0,0]/dt.int64])
    assert_equals(DT_cnt_ngrp, DT_ref)




